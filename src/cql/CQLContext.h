#pragma once
// CQLContext — Evaluation context binding patient data for CQL execution

#include "cql/CQLValue.h"
#include <map>
#include <string>
#include <vector>

namespace hedis {

// ---- Lightweight patient data structs (mirror existing HA types) ----------
// These are self-contained so the CQL engine can be tested without insight/.
// In production, DataAdapter converts insight types → these.

struct HADate {
    int year = 0, month = 0, day = 0;
    bool isNull() const { return year == 0; }
    CQLDate toCQLDate() const { return {year, month, day}; }
};

struct HAPatient {
    std::string patientId;
    std::string gender;       // "male" / "female"
    HADate      birthDate;
    std::string race;
    std::string ethnicity;
};

struct HAClaim {
    std::string claimId;
    std::string type;         // "inpatient", "outpatient", "office"
    HADate      serviceDate;
    HADate      endDate;
    std::string primaryDiagnosis;
    std::string placeOfService;
    std::vector<std::string> procedureCodes;
    std::vector<std::string> diagnosisCodes;
};

struct HACoverage {
    std::string type;         // "medical", "pharmacy", "mental_health"
    HADate      startDate;
    HADate      endDate;
};

struct HADiagnosis {
    std::string code;
    std::string codeSystem;   // "ICD10", "ICD9"
    HADate      date;
    std::string status;
};

struct HAProcedure {
    std::string code;
    std::string codeSystem;   // "CPT", "HCPCS", "SNOMED"
    HADate      date;
    std::string status;
};

struct HADrug {
    std::string ndcCode;
    std::string gpiCode;
    HADate      dispensedDate;
    int         daysSupply = 0;
    double      quantity   = 0.0;
};

struct HALabResult {
    std::string loincCode;
    std::string name;
    HADate      resultDate;
    double      numericValue = 0.0;
    std::string stringValue;
    std::string units;
    std::string status;       // "final", "preliminary"
};

// ---------------------------------------------------------------------------
// CQLContext — binds patient data for CQL evaluation
// ---------------------------------------------------------------------------
class CQLContext {
public:
    CQLContext();
    ~CQLContext();

    // Patient demographics
    void setPatient(const HAPatient& patient);
    const HAPatient& patient() const { return m_patient; }

    // Clinical data arrays
    void setClaims(const std::vector<HAClaim>& claims);
    void setCoverages(const std::vector<HACoverage>& coverages);
    void setDiagnoses(const std::vector<HADiagnosis>& diagnoses);
    void setProcedures(const std::vector<HAProcedure>& procedures);
    void setDrugs(const std::vector<HADrug>& drugs);
    void setLabResults(const std::vector<HALabResult>& labs);

    const std::vector<HAClaim>&     claims()     const { return m_claims; }
    const std::vector<HACoverage>&  coverages()  const { return m_coverages; }
    const std::vector<HADiagnosis>& diagnoses()  const { return m_diagnoses; }
    const std::vector<HAProcedure>& procedures() const { return m_procedures; }
    const std::vector<HADrug>&      drugs()      const { return m_drugs; }
    const std::vector<HALabResult>& labResults() const { return m_labResults; }

    // Parameters (measurement period, run date, etc.)
    void setParameter(const std::string& name, const CQLValue& value);
    CQLValue getParameter(const std::string& name) const;

    // Variable scope for query aliases
    void pushScope();
    void popScope();
    void setVariable(const std::string& name, const CQLValue& value);
    CQLValue getVariable(const std::string& name) const;

    // Patient property accessor — returns CQLValue for "Patient.birthDate" etc.
    CQLValue patientProperty(const std::string& prop) const;

    // Define result cache (evaluated defines are memoized per patient)
    void cacheDefineResult(const std::string& name, const CQLValue& val);
    bool hasDefineResult(const std::string& name) const;
    CQLValue getDefineResult(const std::string& name) const;
    void clearDefineCache();

    // Reset for next patient
    void clear();

private:
    HAPatient                m_patient;
    std::vector<HAClaim>     m_claims;
    std::vector<HACoverage>  m_coverages;
    std::vector<HADiagnosis> m_diagnoses;
    std::vector<HAProcedure> m_procedures;
    std::vector<HADrug>      m_drugs;
    std::vector<HALabResult> m_labResults;

    std::map<std::string, CQLValue> m_parameters;
    std::vector<std::map<std::string, CQLValue>> m_scopeStack;
    std::map<std::string, CQLValue> m_defineCache;
};

}  // namespace hedis
