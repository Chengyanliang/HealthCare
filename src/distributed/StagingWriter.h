#pragma once
// StagingWriter — Bulk write measure results to Oracle staging table

#include "measure/MeasureResult.h"
#include <string>
#include <vector>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

class StagingWriter {
public:
    StagingWriter(OCI_Connection* conn, int partitionId);
    ~StagingWriter();

    // Buffer a result in memory
    void addResult(const MeasureResult& result);

    // Bulk insert buffered results to staging table
    void flush();

    // Commit partition results
    void commit();

    // Number of results written so far
    int writtenCount() const { return m_writtenCount; }

    // Configure batch size (default 1000)
    void setBatchSize(int size) { m_batchSize = size; }

private:
    OCI_Connection* m_conn;
    int m_partitionId;
    std::vector<MeasureResult> m_buffer;
    int m_batchSize    = 1000;
    int m_writtenCount = 0;

    void insertBatch(const std::vector<MeasureResult>& batch);
};

}  // namespace hedis
