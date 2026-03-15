// WorkerPool — Multi-threaded partition processing

#include "engine/WorkerPool.h"
#include "engine/MeasureEngine.h"
#include "data/PatientLoader.h"
#include "distributed/StagingWriter.h"
#include <chrono>
#include <iostream>

namespace hedis {

WorkerPool::WorkerPool(int numThreads, MeasureEngine& engine)
    : m_numThreads(numThreads), m_engine(engine) {}

WorkerPool::~WorkerPool() = default;

// ---------------------------------------------------------------------------
// processPartition — spawn worker threads to process all patients
// ---------------------------------------------------------------------------
void WorkerPool::processPartition(const PartitionInfo& partition,
                                   PatientLoader& loader,
                                   StagingWriter& writer) {
    m_processedCount.store(0);
    m_errorCount.store(0);

    // Load the patient ID range for this partition
    loader.loadPartition(partition.startId, partition.endId);

    std::vector<std::thread> threads;
    threads.reserve(m_numThreads);

    for (int i = 0; i < m_numThreads; ++i) {
        threads.emplace_back(&WorkerPool::workerThread, this,
                             std::ref(loader), std::ref(writer));
    }

    for (auto& t : threads) {
        t.join();
    }

    writer.commit();
}

// ---------------------------------------------------------------------------
// workerThread — fetch patients, evaluate measures, write results
// ---------------------------------------------------------------------------
void WorkerPool::workerThread(PatientLoader& loader, StagingWriter& writer) {
    while (true) {
        // Get next patient
        HAPatient patient;
        std::vector<HAClaim> claims;
        std::vector<HACoverage> coverages;
        std::vector<HADiagnosis> diagnoses;
        std::vector<HAProcedure> procedures;
        std::vector<HADrug> drugs;
        std::vector<HALabResult> labs;

        {
            std::lock_guard<std::mutex> lock(m_loaderMutex);
            if (!loader.loadNextPatient(patient, claims, coverages,
                                         diagnoses, procedures, drugs, labs)) {
                break;  // No more patients
            }
        }

        // Evaluate all measures
        try {
            auto results = m_engine.evaluatePatient(
                patient, claims, coverages, diagnoses, procedures, drugs, labs);

            // Write results to staging
            {
                std::lock_guard<std::mutex> lock(m_writerMutex);
                for (const auto& result : results) {
                    if (result.initialPopulation) {
                        writer.addResult(result);
                    }
                }
            }

            m_processedCount.fetch_add(1);
        } catch (const std::exception& e) {
            m_errorCount.fetch_add(1);
            std::cerr << "Error processing patient " << patient.patientId
                      << ": " << e.what() << std::endl;
        }
    }
}

}  // namespace hedis
