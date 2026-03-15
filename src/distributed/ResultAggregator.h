#pragma once
// ResultAggregator — Post-merge aggregation: compute rates per measure

#include <string>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

class ResultAggregator {
public:
    explicit ResultAggregator(OCI_Connection* conn);
    ~ResultAggregator();

    // Merge staging → production
    void mergeToProduction(const std::string& jobId);

    // Compute rates for all measures in a job
    void computeRates(const std::string& jobId);

    // Verify all partitions are complete before merging
    bool verifyAllComplete(const std::string& jobId) const;

    // Backup production table before merge
    void backupProduction(const std::string& jobId);

private:
    OCI_Connection* m_conn;
};

}  // namespace hedis
