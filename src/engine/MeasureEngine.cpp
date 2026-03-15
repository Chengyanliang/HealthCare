// MeasureEngine — Load measures and evaluate per patient

#include "engine/MeasureEngine.h"
#include "measure/MeasureStore.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hedis {

MeasureEngine::MeasureEngine(const std::string& configFile)
    : m_configFile(configFile) {}

MeasureEngine::~MeasureEngine() = default;

// ---------------------------------------------------------------------------
// initialize — load from Oracle
// ---------------------------------------------------------------------------
void MeasureEngine::initialize(OCI_Connection* conn, const std::string& measurementYear) {
    // Load value sets
    m_valueSetMgr.loadFromDatabase(conn, "HEDIS-" + measurementYear);

    // Load measures
    MeasureStore store(conn);
    m_measures = store.loadActiveMeasures(measurementYear);

    // Set measurement period parameter
    setMeasurementPeriod(measurementYear);
}

// ---------------------------------------------------------------------------
// initializeFromFiles — load from filesystem (for testing)
// ---------------------------------------------------------------------------
void MeasureEngine::initializeFromFiles(const std::string& measuresDir,
                                         const std::string& valueSetsFile,
                                         const std::string& measurementYear) {
    // Load value sets from JSON
    if (!valueSetsFile.empty())
        m_valueSetMgr.loadFromFile(valueSetsFile);

    // Load measures from .cql files
    MeasureStore store(nullptr);
    m_measures = store.loadFromDirectory(measuresDir);

    setMeasurementPeriod(measurementYear);
}

// ---------------------------------------------------------------------------
// setMeasurementPeriod — create the standard interval parameter
// ---------------------------------------------------------------------------
void MeasureEngine::setMeasurementPeriod(const std::string& year) {
    int y = 0;
    try { y = std::stoi(year); } catch (...) { y = 2026; }

    CQLDate start = {y, 1, 1};
    CQLDate end   = {y, 12, 31};
    m_measurementPeriod = CQLValue::fromInterval(
        CQLValue::fromDate(start), CQLValue::fromDate(end));
}

// ---------------------------------------------------------------------------
// evaluatePatient — evaluate all measures for one patient
// ---------------------------------------------------------------------------
std::vector<MeasureResult> MeasureEngine::evaluatePatient(
        const HAPatient& patient,
        const std::vector<HAClaim>& claims,
        const std::vector<HACoverage>& coverages,
        const std::vector<HADiagnosis>& diagnoses,
        const std::vector<HAProcedure>& procedures,
        const std::vector<HADrug>& drugs,
        const std::vector<HALabResult>& labs) {

    // Create evaluator for this invocation
    CQLEvaluator evaluator(m_valueSetMgr);

    // Build context
    CQLContext ctx;
    ctx.setPatient(patient);
    ctx.setClaims(claims);
    ctx.setCoverages(coverages);
    ctx.setDiagnoses(diagnoses);
    ctx.setProcedures(procedures);
    ctx.setDrugs(drugs);
    ctx.setLabResults(labs);
    ctx.setParameter("Measurement Period", m_measurementPeriod);

    // Evaluate each measure
    std::vector<MeasureResult> results;
    results.reserve(m_measures.size());

    for (const auto& measure : m_measures) {
        ctx.clearDefineCache();  // Fresh cache per measure
        MeasureResult result = measure.evaluate(evaluator, ctx);
        results.push_back(std::move(result));
    }

    return results;
}

}  // namespace hedis
