#pragma once
// PartitionManager — Claim/manage Oracle partitions across 24 servers

#include <cstdint>
#include <string>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

struct HADate;  // forward

struct PartitionInfo {
    int partitionId = 0;
    std::string partitionName;
    int64_t startId = 0;
    int64_t endId   = 0;
    std::string analysisMode;
    int runDateYear  = 0;
    int runDateMonth = 0;
    int runDateDay   = 0;
    std::string jobId;
};

struct PartitionStats {
    int totalPatients       = 0;
    int patientsWithCoverage = 0;
    int patientsErrored     = 0;
    int alertsGenerated     = 0;
    double elapsedSeconds   = 0.0;
};

class PartitionManager {
public:
    explicit PartitionManager(OCI_Connection* conn);
    ~PartitionManager();

    // Create partitions (run by main_ctl)
    void createPartitions(int numPartitions, int batchSize,
                          const std::string& jobId,
                          int runYear, int runMonth, int runDay);

    // Claim next available partition (run by each server's main)
    // Uses exclusive table lock — identical to existing pdba_part_driver
    bool claimNextPartition(PartitionInfo& info);

    // Mark partition complete
    void completePartition(int partitionId, const PartitionStats& stats);

    // Mark partition failed
    void failPartition(int partitionId, const std::string& error);

    // Check if all partitions are complete for a job
    bool allPartitionsComplete(const std::string& jobId) const;

    // Get hostname of this server
    const std::string& hostname() const { return m_hostname; }

private:
    OCI_Connection* m_conn;
    std::string m_hostname;

    void resolveHostname();
};

}  // namespace hedis
