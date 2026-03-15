// RetrieveProvider — Maps CQL data types to patient data arrays

#include "cql/RetrieveProvider.h"
#include "measure/ValueSetManager.h"

namespace hedis {

// ---------------------------------------------------------------------------
// Tuple converters — convert HA types to CQL Tuple values
// ---------------------------------------------------------------------------

CQLValue RetrieveProvider::claimToTuple(const HAClaim& claim) {
    CQLTuple t;
    t["id"]           = CQLValue::fromString(claim.claimId);
    t["type"]         = CQLValue::fromString(claim.type);
    t["date"]         = CQLValue::fromDate(claim.serviceDate.toCQLDate());
    t["endDate"]      = CQLValue::fromDate(claim.endDate.toCQLDate());
    t["diagnosis"]    = CQLValue::fromString(claim.primaryDiagnosis);
    t["placeOfService"] = CQLValue::fromString(claim.placeOfService);

    // Build code list
    CQLList codes;
    for (const auto& pc : claim.procedureCodes)
        codes.push_back(CQLValue::fromCode({pc, "CPT", ""}));
    for (const auto& dc : claim.diagnosisCodes)
        codes.push_back(CQLValue::fromCode({dc, "ICD10", ""}));
    t["codes"] = CQLValue::fromList(codes);

    // Period as interval
    t["period"] = CQLValue::fromInterval(
        CQLValue::fromDate(claim.serviceDate.toCQLDate()),
        CQLValue::fromDate(claim.endDate.toCQLDate()));

    return CQLValue::fromTuple(t);
}

CQLValue RetrieveProvider::diagnosisToTuple(const HADiagnosis& dx) {
    CQLTuple t;
    t["code"]       = CQLValue::fromCode({dx.code, dx.codeSystem, ""});
    t["codeSystem"] = CQLValue::fromString(dx.codeSystem);
    t["date"]       = CQLValue::fromDate(dx.date.toCQLDate());
    t["status"]     = CQLValue::fromString(dx.status);
    return CQLValue::fromTuple(t);
}

CQLValue RetrieveProvider::procedureToTuple(const HAProcedure& px) {
    CQLTuple t;
    t["code"]       = CQLValue::fromCode({px.code, px.codeSystem, ""});
    t["codeSystem"] = CQLValue::fromString(px.codeSystem);
    t["date"]       = CQLValue::fromDate(px.date.toCQLDate());
    t["status"]     = CQLValue::fromString(px.status);
    return CQLValue::fromTuple(t);
}

CQLValue RetrieveProvider::drugToTuple(const HADrug& drug) {
    CQLTuple t;
    t["code"]       = CQLValue::fromCode({drug.ndcCode, "NDC", ""});
    t["gpiCode"]    = CQLValue::fromString(drug.gpiCode);
    t["date"]       = CQLValue::fromDate(drug.dispensedDate.toCQLDate());
    t["daysSupply"] = CQLValue::fromInt(drug.daysSupply);
    t["quantity"]   = CQLValue::fromDecimal(drug.quantity);
    return CQLValue::fromTuple(t);
}

CQLValue RetrieveProvider::labToTuple(const HALabResult& lab) {
    CQLTuple t;
    t["code"]         = CQLValue::fromCode({lab.loincCode, "LOINC", lab.name});
    t["date"]         = CQLValue::fromDate(lab.resultDate.toCQLDate());
    t["numericValue"] = CQLValue::fromDecimal(lab.numericValue);
    t["stringValue"]  = CQLValue::fromString(lab.stringValue);
    t["units"]        = CQLValue::fromString(lab.units);
    t["status"]       = CQLValue::fromString(lab.status);
    return CQLValue::fromTuple(t);
}

CQLValue RetrieveProvider::coverageToTuple(const HACoverage& cov) {
    CQLTuple t;
    t["type"]      = CQLValue::fromString(cov.type);
    t["startDate"] = CQLValue::fromDate(cov.startDate.toCQLDate());
    t["endDate"]   = CQLValue::fromDate(cov.endDate.toCQLDate());
    t["period"]    = CQLValue::fromInterval(
        CQLValue::fromDate(cov.startDate.toCQLDate()),
        CQLValue::fromDate(cov.endDate.toCQLDate()));
    return CQLValue::fromTuple(t);
}

// ---------------------------------------------------------------------------
// retrieve — the main mapping function
// ---------------------------------------------------------------------------
CQLValue RetrieveProvider::retrieve(const std::string& dataType,
                                     const std::string& valueSetName,
                                     const CQLContext& ctx,
                                     const ValueSetManager& vsMgr) const {
    CQLList results;

    if (dataType == "Encounter" || dataType == "Claim") {
        for (const auto& claim : ctx.claims()) {
            if (valueSetName.empty()) {
                results.push_back(claimToTuple(claim));
                continue;
            }
            // Check if any code on the claim is in the value set
            bool matched = false;
            for (const auto& pc : claim.procedureCodes) {
                if (vsMgr.isMember(valueSetName, pc, "CPT") ||
                    vsMgr.isMember(valueSetName, pc, "HCPCS")) {
                    matched = true; break;
                }
            }
            if (!matched) {
                for (const auto& dc : claim.diagnosisCodes) {
                    if (vsMgr.isMember(valueSetName, dc, "ICD10")) {
                        matched = true; break;
                    }
                }
            }
            if (matched) results.push_back(claimToTuple(claim));
        }
    }
    else if (dataType == "Condition" || dataType == "Diagnosis") {
        for (const auto& dx : ctx.diagnoses()) {
            if (valueSetName.empty() ||
                vsMgr.isMember(valueSetName, dx.code, dx.codeSystem)) {
                results.push_back(diagnosisToTuple(dx));
            }
        }
    }
    else if (dataType == "Procedure") {
        for (const auto& px : ctx.procedures()) {
            if (valueSetName.empty() ||
                vsMgr.isMember(valueSetName, px.code, px.codeSystem)) {
                results.push_back(procedureToTuple(px));
            }
        }
        // Also check claims for procedure codes
        for (const auto& claim : ctx.claims()) {
            for (const auto& pc : claim.procedureCodes) {
                if (valueSetName.empty() ||
                    vsMgr.isMember(valueSetName, pc, "CPT") ||
                    vsMgr.isMember(valueSetName, pc, "HCPCS")) {
                    // Create a procedure-like tuple from the claim
                    CQLTuple t;
                    t["code"] = CQLValue::fromCode({pc, "CPT", ""});
                    t["date"] = CQLValue::fromDate(claim.serviceDate.toCQLDate());
                    t["status"] = CQLValue::fromString("completed");
                    results.push_back(CQLValue::fromTuple(t));
                    break;  // one match per claim
                }
            }
        }
    }
    else if (dataType == "MedicationOrder" || dataType == "MedicationDispense" ||
             dataType == "Medication") {
        for (const auto& drug : ctx.drugs()) {
            if (valueSetName.empty() ||
                vsMgr.isMember(valueSetName, drug.ndcCode, "NDC") ||
                vsMgr.isMember(valueSetName, drug.gpiCode, "GPI")) {
                results.push_back(drugToTuple(drug));
            }
        }
    }
    else if (dataType == "Observation" || dataType == "LaboratoryTest") {
        for (const auto& lab : ctx.labResults()) {
            if (valueSetName.empty() ||
                vsMgr.isMember(valueSetName, lab.loincCode, "LOINC") ||
                vsMgr.isMember(valueSetName, lab.loincCode, "CPT")) {
                results.push_back(labToTuple(lab));
            }
        }
    }
    else if (dataType == "Coverage") {
        for (const auto& cov : ctx.coverages()) {
            results.push_back(coverageToTuple(cov));
        }
    }

    return CQLValue::fromList(results);
}

}  // namespace hedis
