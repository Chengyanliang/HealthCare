#pragma once
// WorkerPool — Thread pool for per-partition patient processing

#include "distributed/PartitionManager.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace hedis {

class MeasureEngine;
class StagingWriter;
class PatientLoader;

class WorkerPool {
public:
    WorkerPool(int numThreads, MeasureEngine& engine);
    ~WorkerPool();

    // Process all patients in a partition
    // Loads patients from Oracle, evaluates all measures, writes to staging
    void processPartition(const PartitionInfo& partition,
                          PatientLoader& loader,
                          StagingWriter& writer);

    // Statistics from last partition
    int processedCount() const { return m_processedCount.load(); }
    int errorCount()     const { return m_errorCount.load(); }

private:
    int m_numThreads;
    MeasureEngine& m_engine;

    std::atomic<int> m_processedCount{0};
    std::atomic<int> m_errorCount{0};
    std::mutex m_loaderMutex;   // Protect patient loader (single reader)
    std::mutex m_writerMutex;   // Protect staging writer

    // Worker thread function
    void workerThread(PatientLoader& loader,
                      StagingWriter& writer);
};

}  // namespace hedis
