// DataAdapter — Type conversion helpers

#include "data/DataAdapter.h"

namespace hedis {

HAPatient DataAdapter::toPatient(const std::string& memberId,
                                  const std::string& gender,
                                  int birthYear, int birthMonth, int birthDay,
                                  const std::string& race,
                                  const std::string& ethnicity) {
    HAPatient p;
    p.patientId = memberId;
    p.gender    = gender;
    p.birthDate = {birthYear, birthMonth, birthDay};
    p.race      = race;
    p.ethnicity = ethnicity;
    return p;
}

HAClaim DataAdapter::toClaim(const std::string& claimId,
                              const std::string& claimType,
                              int svcYear, int svcMonth, int svcDay,
                              int endYear, int endMonth, int endDay,
                              const std::string& primaryDx,
                              const std::string& pos) {
    HAClaim c;
    c.claimId          = claimId;
    c.type             = claimType;
    c.serviceDate      = {svcYear, svcMonth, svcDay};
    c.endDate          = {endYear, endMonth, endDay};
    c.primaryDiagnosis = primaryDx;
    c.placeOfService   = pos;
    return c;
}

HACoverage DataAdapter::toCoverage(const std::string& type,
                                    int startYear, int startMonth, int startDay,
                                    int endYear, int endMonth, int endDay) {
    HACoverage cov;
    cov.type      = type;
    cov.startDate = {startYear, startMonth, startDay};
    cov.endDate   = {endYear, endMonth, endDay};
    return cov;
}

HADiagnosis DataAdapter::toDiagnosis(const std::string& code,
                                      const std::string& codeSystem,
                                      int year, int month, int day,
                                      const std::string& status) {
    HADiagnosis dx;
    dx.code       = code;
    dx.codeSystem = codeSystem;
    dx.date       = {year, month, day};
    dx.status     = status;
    return dx;
}

HAProcedure DataAdapter::toProcedure(const std::string& code,
                                      const std::string& codeSystem,
                                      int year, int month, int day,
                                      const std::string& status) {
    HAProcedure px;
    px.code       = code;
    px.codeSystem = codeSystem;
    px.date       = {year, month, day};
    px.status     = status;
    return px;
}

HADrug DataAdapter::toDrug(const std::string& ndcCode,
                            const std::string& gpiCode,
                            int year, int month, int day,
                            int daysSupply, double quantity) {
    HADrug d;
    d.ndcCode       = ndcCode;
    d.gpiCode       = gpiCode;
    d.dispensedDate  = {year, month, day};
    d.daysSupply    = daysSupply;
    d.quantity      = quantity;
    return d;
}

HALabResult DataAdapter::toLabResult(const std::string& loincCode,
                                      const std::string& name,
                                      int year, int month, int day,
                                      double numericValue,
                                      const std::string& units,
                                      const std::string& status) {
    HALabResult lab;
    lab.loincCode    = loincCode;
    lab.name         = name;
    lab.resultDate   = {year, month, day};
    lab.numericValue = numericValue;
    lab.units        = units;
    lab.status       = status;
    return lab;
}

}  // namespace hedis
