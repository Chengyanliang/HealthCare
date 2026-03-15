#pragma once
// RetrieveProvider — Maps CQL retrieve expressions to patient data arrays

#include "cql/CQLContext.h"
#include "cql/CQLValue.h"
#include <string>

namespace hedis {

class ValueSetManager;  // forward declaration

class RetrieveProvider {
public:
    // CQL: [Encounter: "Office Visit"]         → filter claims by value set
    // CQL: [Condition: "Diabetes"]              → filter diagnoses by value set
    // CQL: [Procedure: "Cervical Cytology"]     → filter procedures by value set
    // CQL: [MedicationOrder: "Antidiabetics"]   → filter drugs by value set
    // CQL: [Observation: "HbA1c"]               → filter lab results by value set
    // CQL: [Coverage]                           → return coverages
    CQLValue retrieve(const std::string& dataType,
                      const std::string& valueSetName,
                      const CQLContext& ctx,
                      const ValueSetManager& vsMgr) const;

private:
    // Convert a claim to a CQL Tuple
    static CQLValue claimToTuple(const HAClaim& claim);

    // Convert a diagnosis to a CQL Tuple
    static CQLValue diagnosisToTuple(const HADiagnosis& dx);

    // Convert a procedure to a CQL Tuple
    static CQLValue procedureToTuple(const HAProcedure& px);

    // Convert a drug to a CQL Tuple
    static CQLValue drugToTuple(const HADrug& drug);

    // Convert a lab result to a CQL Tuple
    static CQLValue labToTuple(const HALabResult& lab);

    // Convert a coverage to a CQL Tuple
    static CQLValue coverageToTuple(const HACoverage& cov);
};

}  // namespace hedis
