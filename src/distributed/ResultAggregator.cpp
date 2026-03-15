// ResultAggregator — Merge staging and compute rates

#include "distributed/ResultAggregator.h"

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace hedis {

ResultAggregator::ResultAggregator(OCI_Connection* conn) : m_conn(conn) {}
ResultAggregator::~ResultAggregator() = default;

// ---------------------------------------------------------------------------
// verifyAllComplete
// ---------------------------------------------------------------------------
bool ResultAggregator::verifyAllComplete(const std::string& jobId) const {
#ifdef HAS_OCILIB
    if (!m_conn) return false;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "SELECT COUNT(*) FROM SMA_PARTITIONS_CTL "
        "WHERE JOB_ID = '%s' AND STATUS != 'C'", jobId.c_str());
    OCI_Resultset* rs = OCI_GetResultset(stmt);
    int remaining = 0;
    if (OCI_FetchNext(rs)) remaining = OCI_GetInt(rs, 1);
    OCI_StatementFree(stmt);
    return remaining == 0;
#else
    (void)jobId;
    return true;
#endif
}

// ---------------------------------------------------------------------------
// backupProduction
// ---------------------------------------------------------------------------
void ResultAggregator::backupProduction(const std::string& jobId) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);
    OCI_ExecuteStmtFmt(stmt,
        "CREATE TABLE SMA_CQL_RESULTS_BAK_%s AS "
        "SELECT * FROM SMA_CQL_RESULTS", jobId.c_str());
    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)jobId;
#endif
}

// ---------------------------------------------------------------------------
// mergeToProduction — move staging → production
// ---------------------------------------------------------------------------
void ResultAggregator::mergeToProduction(const std::string& jobId) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);

    // Merge: update existing, insert new
    OCI_ExecuteStmtFmt(stmt,
        "MERGE INTO SMA_CQL_RESULTS t "
        "USING SMA_CQL_RESULTS_STG s "
        "ON (t.PATIENT_ID = s.PATIENT_ID AND t.MEASURE_ID = s.MEASURE_ID) "
        "WHEN MATCHED THEN UPDATE SET "
        "  t.INITIAL_POP = s.INITIAL_POP, "
        "  t.DENOMINATOR = s.DENOMINATOR, "
        "  t.NUMERATOR = s.NUMERATOR, "
        "  t.DENOM_EXCLUSION = s.DENOM_EXCLUSION, "
        "  t.FAILURE_DATE = s.FAILURE_DATE, "
        "  t.NUMERIC_RESULT = s.NUMERIC_RESULT, "
        "  t.JOB_ID = '%s', "
        "  t.PROCESS_DATE = s.PROCESS_DATE "
        "WHEN NOT MATCHED THEN INSERT "
        "  (PATIENT_ID, MEASURE_ID, INITIAL_POP, DENOMINATOR, NUMERATOR, "
        "   DENOM_EXCLUSION, FAILURE_DATE, NUMERIC_RESULT, JOB_ID, PROCESS_DATE) "
        "VALUES "
        "  (s.PATIENT_ID, s.MEASURE_ID, s.INITIAL_POP, s.DENOMINATOR, s.NUMERATOR, "
        "   s.DENOM_EXCLUSION, s.FAILURE_DATE, s.NUMERIC_RESULT, '%s', s.PROCESS_DATE)",
        jobId.c_str(), jobId.c_str());

    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)jobId;
#endif
}

// ---------------------------------------------------------------------------
// computeRates — aggregate per-measure rates
// ---------------------------------------------------------------------------
void ResultAggregator::computeRates(const std::string& jobId) {
#ifdef HAS_OCILIB
    if (!m_conn) return;
    OCI_Statement* stmt = OCI_StatementCreate(m_conn);

    // Delete old rates for this job
    OCI_ExecuteStmtFmt(stmt,
        "DELETE FROM SMA_CQL_MEASURE_RATES WHERE JOB_ID = '%s'",
        jobId.c_str());

    // Compute and insert
    OCI_ExecuteStmtFmt(stmt,
        "INSERT INTO SMA_CQL_MEASURE_RATES "
        "(JOB_ID, MEASURE_ID, INITIAL_POP_COUNT, DENOM_COUNT, "
        " NUMER_COUNT, EXCLUSION_COUNT, RATE, PROCESS_DATE) "
        "SELECT '%s', MEASURE_ID, "
        "  SUM(CASE WHEN INITIAL_POP = 'Y' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN DENOMINATOR = 'Y' AND DENOM_EXCLUSION != 'Y' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN NUMERATOR = 'Y' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN DENOM_EXCLUSION = 'Y' THEN 1 ELSE 0 END), "
        "  ROUND(SUM(CASE WHEN NUMERATOR = 'Y' THEN 1.0 ELSE 0 END) / "
        "    NULLIF(SUM(CASE WHEN DENOMINATOR = 'Y' AND DENOM_EXCLUSION != 'Y' "
        "                     THEN 1.0 ELSE 0 END), 0) * 100, 2), "
        "  SYSDATE "
        "FROM SMA_CQL_RESULTS "
        "WHERE JOB_ID = '%s' "
        "GROUP BY MEASURE_ID",
        jobId.c_str(), jobId.c_str());

    OCI_Commit(m_conn);
    OCI_StatementFree(stmt);
#else
    (void)jobId;
#endif
}

}  // namespace hedis
