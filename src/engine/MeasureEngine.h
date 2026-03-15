#pragma once
// MeasureEngine — Orchestrator: loads measures, evaluates per patient

#include "cql/CQLEvaluator.h"
#include "cql/CQLParser.h"
#include "measure/MeasureDefinition.h"
#include "measure/MeasureResult.h"
#include "measure/ValueSetManager.h"
#include <memory>
#include <string>
#include <vector>

typedef struct OCI_Connection OCI_Connection;

namespace hedis {

class MeasureEngine {
public:
    explicit MeasureEngine(const std::string& configFile);
    ~MeasureEngine();

    // Initialize: load measures, value sets, parser
    void initialize(OCI_Connection* conn, const std::string& measurementYear);

    // Initialize from filesystem (for testing without Oracle)
    void initializeFromFiles(const std::string& measuresDir,
                              const std::string& valueSetsFile,
                              const std::string& measurementYear);

    // Evaluate all measures for one patient
    // Called per worker thread — each thread creates its own evaluator
    std::vector<MeasureResult> evaluatePatient(
        const HAPatient& patient,
        const std::vector<HAClaim>& claims,
        const std::vector<HACoverage>& coverages,
        const std::vector<HADiagnosis>& diagnoses,
        const std::vector<HAProcedure>& procedures,
        const std::vector<HADrug>& drugs,
        const std::vector<HALabResult>& labs);

    // Number of loaded measures
    size_t measureCount() const { return m_measures.size(); }

    // Access value set manager (for evaluators)
    const ValueSetManager& valueSetManager() const { return m_valueSetMgr; }

private:
    std::string m_configFile;
    std::vector<MeasureDefinition> m_measures;  // Pre-parsed
    ValueSetManager m_valueSetMgr;
    CQLMeasureParser m_parser;

    // Measurement period interval (shared parameter)
    CQLValue m_measurementPeriod;

    void setMeasurementPeriod(const std::string& year);
};

}  // namespace hedis
