#pragma once
// PatientLoader — Load patient data from Oracle partitions

#include "cql/CQLContext.h"
#include <cstdint>
#include <string>
#include <vector>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

class PatientLoader {
public:
    explicit PatientLoader(OCI_Connection* conn);
    ~PatientLoader();

    // Prepare to iterate over a patient ID range
    void loadPartition(int64_t startId, int64_t endId);

    // Load the next patient and all associated data
    // Returns false when no more patients
    bool loadNextPatient(HAPatient& patient,
                         std::vector<HAClaim>& claims,
                         std::vector<HACoverage>& coverages,
                         std::vector<HADiagnosis>& diagnoses,
                         std::vector<HAProcedure>& procedures,
                         std::vector<HADrug>& drugs,
                         std::vector<HALabResult>& labs);

    // Total patients loaded so far
    int loadedCount() const { return m_loadedCount; }

private:
    OCI_Connection* m_conn;
    int64_t m_startId = 0;
    int64_t m_endId   = 0;
    int64_t m_currentId = 0;
    int m_loadedCount = 0;

    // Pre-fetched patient IDs for this partition
    std::vector<std::string> m_patientIds;
    size_t m_patientIndex = 0;

    void fetchPatientIds();
    void loadPatientDemographics(const std::string& patientId, HAPatient& patient);
    void loadClaims(const std::string& patientId, std::vector<HAClaim>& claims);
    void loadCoverages(const std::string& patientId, std::vector<HACoverage>& coverages);
    void loadDiagnoses(const std::string& patientId, std::vector<HADiagnosis>& diagnoses);
    void loadProcedures(const std::string& patientId, std::vector<HAProcedure>& procedures);
    void loadDrugs(const std::string& patientId, std::vector<HADrug>& drugs);
    void loadLabResults(const std::string& patientId, std::vector<HALabResult>& labs);
};

}  // namespace hedis
