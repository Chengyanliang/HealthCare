// PartitionManager — Oracle partition claiming and lifecycle

#include "distributed/PartitionManager.h"
#include <cstring>
#include <unistd.h>

// #include <ocilib.h>

namespace hedis {

PartitionManager::PartitionManager(OCI_Connection* conn)
    : m_conn(conn) {
    resolveHostname();
}

PartitionManager::~PartitionManager() = default;

void PartitionManager::resolveHostname() {
    char buf[256] = {};
    gethostname(buf, sizeof(buf));
    m_hostname = buf;
}

// ---------------------------------------------------------------------------
// createPartitions — create partition records in SMA_PARTITIONS_CTL
// Called once by hedis_cql_ctl before workers start
// ---------------------------------------------------------------------------
void PartitionManager::createPartitions(int numPartitions, int batchSize,
                                         const std::string& jobId,
                                         int runYear, int runMonth, int runDay) {
#ifdef HAS_OCILIB
    if (!m_conn) return;

    // Get total patient count
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmt(stmt, "SELECT COUNT(*) FROM SMA_MEMBER_MASTER");
    OCI_Resultset* rs = OCI_GetResultset(stmt);
    int64_t totalPatients = 0;
    if (OCI_FetchNext(rs)) totalPatients = OCI_GetBigInt(rs, 1);
    OCI_StatementFree(stmt);

    int64_t patientsPerPartition = (totalPatients + numPartitions - 1) / numPartitions;

    // Create partition records
    for (int i = 0; i < numPartitions; ++i) {
        int64_t startId = i * patientsPerPartition + 1;
        int64_t endId   = std::min((i + 1) * patientsPerPartition, totalPatients);

        stmt = OCI_StatementCreate(m_conn);
        OCI_ExecuteStmtFmt(stmt,
            "INSERT INTO SMA_PARTITIONS_CTL "
            "(PARTITION_ID, JOB_ID, START_ID, END_ID, STATUS, "
            " RUN_DATE, BATCH_SIZE, CREATED_DATE) "
            "VALUES (%d, '%s', %lld, %lld, 'P', "
            " TO_DATE('%04d%02d%02d','YYYYMMDD'), %d, SYSDATE)",
            i + 1, jobId.c_str(), startId, endId,
            runYear, runMonth, runDay, batchSize);
        OCI_StatementFree(stmt);
    }
    OCI_Commit(m_conn);

    // Insert job parameters
    stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "INSERT INTO SMA_PARTITIONS_PARAMS "
        "(JOB_ID, PARAM_NAME, PARAM_VALUE) "
        "VALUES ('%s', 'NUM_PARTITIONS', '%d')",
        jobId.c_str(), numPartitions);
    OCI_StatementFree(stmt);
    OCI_Commit(m_conn);
#else
    (void)numPartitions; (void)batchSize;
    (void)jobId; (void)runYear; (void)runMonth; (void)runDay;
#endif
}

// ---------------------------------------------------------------------------
// claimNextPartition — atomically claim the next available partition
// Uses exclusive lock to prevent duplicate claiming across servers
// ---------------------------------------------------------------------------
bool PartitionManager::claimNextPartition(PartitionInfo& info) {
#ifdef HAS_OCILIB
    if (!m_conn) return false;

    // Lock the control table
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmt(stmt, "LOCK TABLE SMA_PARTITIONS_CTL IN EXCLUSIVE MODE");

    // Find next pending partition
    OCI_ExecuteStmt(stmt,
        "SELECT PARTITION_ID, START_ID, END_ID, JOB_ID "
        "FROM SMA_PARTITIONS_CTL "
        "WHERE STATUS = 'P' "
        "ORDER BY PARTITION_ID "
        "FETCH FIRST 1 ROW ONLY");

    OCI_Resultset* rs = OCI_GetResultset(stmt);
    if (!OCI_FetchNext(rs)) {
        OCI_StatementFree(stmt);
        OCI_Commit(m_conn);  // Release lock
        return false;
    }

    info.partitionId = OCI_GetInt(rs, 1);
    info.startId     = OCI_GetBigInt(rs, 2);
    info.endId       = OCI_GetBigInt(rs, 3);
    info.jobId       = OCI_GetString(rs, 4) ? OCI_GetString(rs, 4) : "";

    // Mark as running
    OCI_ExecuteStmtFmt(stmt,
        "UPDATE SMA_PARTITIONS_CTL SET STATUS = 'R', "
        "HOSTNAME = '%s', START_TIME = SYSDATE "
        "WHERE PARTITION_ID = %d",
        m_hostname.c_str(), info.partitionId);

    OCI_Commit(m_conn);  // Release lock
    OCI_StatementFree(stmt);
    return true;
#else
    (void)info;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// completePartition — mark a partition as complete with stats
// ---------------------------------------------------------------------------
void PartitionManager::completePartition(int partitionId, const PartitionStats& stats) {
#ifdef HAS_OCILIB
    if (!m_conn) return;

    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "UPDATE SMA_PARTITIONS_CTL SET STATUS = 'C', "
        "END_TIME = SYSDATE, "
        "TOTAL_PATIENTS = %d, COVERED_PATIENTS = %d, "
        "ERROR_PATIENTS = %d, ALERTS = %d, ELAPSED_SEC = %.2f "
        "WHERE PARTITION_ID = %d",
        stats.totalPatients, stats.patientsWithCoverage,
        stats.patientsErrored, stats.alertsGenerated,
        stats.elapsedSeconds, partitionId);
    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)partitionId; (void)stats;
#endif
}

// ---------------------------------------------------------------------------
// failPartition — mark a partition as failed
// ---------------------------------------------------------------------------
void PartitionManager::failPartition(int partitionId, const std::string& error) {
#ifdef HAS_OCILIB
    if (!m_conn) return;

    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "UPDATE SMA_PARTITIONS_CTL SET STATUS = 'F', "
        "END_TIME = SYSDATE, ERROR_MSG = '%.200s' "
        "WHERE PARTITION_ID = %d",
        error.c_str(), partitionId);
    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)partitionId; (void)error;
#endif
}

// ---------------------------------------------------------------------------
// allPartitionsComplete
// ---------------------------------------------------------------------------
bool PartitionManager::allPartitionsComplete(const std::string& jobId) const {
#ifdef HAS_OCILIB
    if (!m_conn) return false;

    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "SELECT COUNT(*) FROM SMA_PARTITIONS_CTL "
        "WHERE JOB_ID = '%s' AND STATUS != 'C'",
        jobId.c_str());
    OCI_Resultset* rs = OCI_GetResultset(stmt);
    int remaining = 0;
    if (OCI_FetchNext(rs)) remaining = OCI_GetInt(rs, 1);
    OCI_StatementFree(stmt);
    return remaining == 0;
#else
    (void)jobId;
    return false;
#endif
}

}  // namespace hedis
