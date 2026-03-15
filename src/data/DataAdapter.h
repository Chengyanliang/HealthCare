#pragma once
// DataAdapter — Convert between Oracle row data and CQL-compatible structures
// Bridges existing insight/ types (if available) to hedis-cql types

#include "cql/CQLContext.h"
#include "cql/CQLValue.h"
#include <string>
#include <vector>

namespace hedis {

class DataAdapter {
public:
    // Convert a single Oracle result row to an HAPatient
    // Column mapping: MEMBER_ID, GENDER, BIRTH_DATE, RACE, ETHNICITY
    static HAPatient toPatient(const std::string& memberId,
                                const std::string& gender,
                                int birthYear, int birthMonth, int birthDay,
                                const std::string& race = "",
                                const std::string& ethnicity = "");

    // Convert claim-level data to HAClaim
    static HAClaim toClaim(const std::string& claimId,
                            const std::string& claimType,
                            int svcYear, int svcMonth, int svcDay,
                            int endYear, int endMonth, int endDay,
                            const std::string& primaryDx = "",
                            const std::string& pos = "");

    // Convert coverage data
    static HACoverage toCoverage(const std::string& type,
                                  int startYear, int startMonth, int startDay,
                                  int endYear, int endMonth, int endDay);

    // Convert diagnosis data
    static HADiagnosis toDiagnosis(const std::string& code,
                                    const std::string& codeSystem,
                                    int year, int month, int day,
                                    const std::string& status = "active");

    // Convert procedure data
    static HAProcedure toProcedure(const std::string& code,
                                    const std::string& codeSystem,
                                    int year, int month, int day,
                                    const std::string& status = "completed");

    // Convert drug data
    static HADrug toDrug(const std::string& ndcCode,
                          const std::string& gpiCode,
                          int year, int month, int day,
                          int daysSupply = 0,
                          double quantity = 0.0);

    // Convert lab result data
    static HALabResult toLabResult(const std::string& loincCode,
                                    const std::string& name,
                                    int year, int month, int day,
                                    double numericValue,
                                    const std::string& units = "",
                                    const std::string& status = "final");
};

}  // namespace hedis
