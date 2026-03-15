// StagingWriter — Bulk insert results to SMA_CQL_RESULTS_STG

#include "distributed/StagingWriter.h"

// #include <ocilib.h>

namespace hedis {

StagingWriter::StagingWriter(OCI_Connection* conn, int partitionId)
    : m_conn(conn), m_partitionId(partitionId) {}

StagingWriter::~StagingWriter() {
    // Flush any remaining buffered results
    if (!m_buffer.empty()) {
        try { flush(); } catch (...) {}
    }
}

// ---------------------------------------------------------------------------
// addResult — buffer a result, flush when batch size reached
// ---------------------------------------------------------------------------
void StagingWriter::addResult(const MeasureResult& result) {
    m_buffer.push_back(result);
    if (static_cast<int>(m_buffer.size()) >= m_batchSize)
        flush();
}

// ---------------------------------------------------------------------------
// flush — bulk insert the buffer to Oracle
// ---------------------------------------------------------------------------
void StagingWriter::flush() {
    if (m_buffer.empty()) return;

    insertBatch(m_buffer);
    m_writtenCount += static_cast<int>(m_buffer.size());
    m_buffer.clear();
}

// ---------------------------------------------------------------------------
// commit — final commit for the partition
// ---------------------------------------------------------------------------
void StagingWriter::commit() {
    flush();

#ifdef HAS_OCILIB
    if (m_conn) OCI_Commit(m_conn);
#endif
}

// ---------------------------------------------------------------------------
// insertBatch — the actual Oracle insert
// ---------------------------------------------------------------------------
void StagingWriter::insertBatch(const std::vector<MeasureResult>& batch) {
#ifdef HAS_OCILIB
    if (!m_conn || batch.empty()) return;

    OCI_Statement* stmt = OCI_StatementCreate(m_conn);

    for (const auto& r : batch) {
        OCI_ExecuteStmtFmt(stmt,
            "INSERT INTO SMA_CQL_RESULTS_STG "
            "(PARTITION_ID, PATIENT_ID, MEASURE_ID, "
            " INITIAL_POP, DENOMINATOR, NUMERATOR, "
            " DENOM_EXCLUSION, DENOM_EXCEPTION, "
            " FAILURE_DATE, NUMERIC_RESULT) "
            "VALUES (%d, '%s', '%s', '%c', '%c', '%c', '%c', '%c', "
            " %s, %.4f)",
            m_partitionId,
            r.patientId.c_str(),
            r.measureId.c_str(),
            r.initialPopulation    ? 'Y' : 'N',
            r.denominator          ? 'Y' : 'N',
            r.numerator            ? 'Y' : 'N',
            r.denominatorExclusion ? 'Y' : 'N',
            r.denominatorException ? 'Y' : 'N',
            r.failureDate.isNull() ? "NULL"
                : ("TO_DATE('" +
                   std::to_string(r.failureDate.year * 10000 +
                                  r.failureDate.month * 100 +
                                  r.failureDate.day) +
                   "','YYYYMMDD')").c_str(),
            r.numericResult);
    }

    OCI_StatementFree(stmt);
#else
    (void)batch;
#endif
}

}  // namespace hedis
