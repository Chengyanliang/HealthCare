// CQLContext — Patient data binding for CQL evaluation

#include "cql/CQLContext.h"

namespace hedis {

CQLContext::CQLContext()  = default;
CQLContext::~CQLContext() = default;

void CQLContext::setPatient(const HAPatient& patient) { m_patient = patient; }
void CQLContext::setClaims(const std::vector<HAClaim>& claims) { m_claims = claims; }
void CQLContext::setCoverages(const std::vector<HACoverage>& coverages) { m_coverages = coverages; }
void CQLContext::setDiagnoses(const std::vector<HADiagnosis>& diagnoses) { m_diagnoses = diagnoses; }
void CQLContext::setProcedures(const std::vector<HAProcedure>& procedures) { m_procedures = procedures; }
void CQLContext::setDrugs(const std::vector<HADrug>& drugs) { m_drugs = drugs; }
void CQLContext::setLabResults(const std::vector<HALabResult>& labs) { m_labResults = labs; }

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------
void CQLContext::setParameter(const std::string& name, const CQLValue& value) {
    m_parameters[name] = value;
}

CQLValue CQLContext::getParameter(const std::string& name) const {
    auto it = m_parameters.find(name);
    return (it != m_parameters.end()) ? it->second : CQLValue::null();
}

// ---------------------------------------------------------------------------
// Variable scope (for query aliases)
// ---------------------------------------------------------------------------
void CQLContext::pushScope() {
    m_scopeStack.emplace_back();
}

void CQLContext::popScope() {
    if (!m_scopeStack.empty())
        m_scopeStack.pop_back();
}

void CQLContext::setVariable(const std::string& name, const CQLValue& value) {
    if (!m_scopeStack.empty())
        m_scopeStack.back()[name] = value;
}

CQLValue CQLContext::getVariable(const std::string& name) const {
    // Search scopes from innermost to outermost
    for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return CQLValue::null();
}

// ---------------------------------------------------------------------------
// Patient property accessor
// ---------------------------------------------------------------------------
CQLValue CQLContext::patientProperty(const std::string& prop) const {
    if (prop == "id" || prop == "patientId")
        return CQLValue::fromString(m_patient.patientId);
    if (prop == "gender" || prop == "sex")
        return CQLValue::fromString(m_patient.gender);
    if (prop == "birthDate" || prop == "dateOfBirth")
        return CQLValue::fromDate(m_patient.birthDate.toCQLDate());
    if (prop == "race")
        return CQLValue::fromString(m_patient.race);
    if (prop == "ethnicity")
        return CQLValue::fromString(m_patient.ethnicity);
    return CQLValue::null();
}

// ---------------------------------------------------------------------------
// Define result cache
// ---------------------------------------------------------------------------
void CQLContext::cacheDefineResult(const std::string& name, const CQLValue& val) {
    m_defineCache[name] = val;
}

bool CQLContext::hasDefineResult(const std::string& name) const {
    return m_defineCache.find(name) != m_defineCache.end();
}

CQLValue CQLContext::getDefineResult(const std::string& name) const {
    auto it = m_defineCache.find(name);
    return (it != m_defineCache.end()) ? it->second : CQLValue::null();
}

void CQLContext::clearDefineCache() {
    m_defineCache.clear();
}

// ---------------------------------------------------------------------------
// Reset for next patient
// ---------------------------------------------------------------------------
void CQLContext::clear() {
    m_patient = HAPatient();
    m_claims.clear();
    m_coverages.clear();
    m_diagnoses.clear();
    m_procedures.clear();
    m_drugs.clear();
    m_labResults.clear();
    m_scopeStack.clear();
    m_defineCache.clear();
    // Parameters are preserved (measurement period doesn't change per patient)
}

}  // namespace hedis
