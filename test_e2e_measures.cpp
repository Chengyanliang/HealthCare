// test_e2e_measures — End-to-end test: load 10 FHIR patients from Oracle,
//                     evaluate all 5 HEDIS measures, compare against expected outcomes
//
// Usage: ./test_e2e_measures [-w wallet_dir]
// Env:   HEDIS_DB_PASSWORD

#include "engine/MeasureEngine.h"
#include "data/PatientLoader.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

// Expected outcome per (patientId, measureId)
// Values: "Compliant", "Non-Compl", "Excluded", "N/A"
struct Expected {
    std::string patientId;
    int age;
    char sex;
    std::map<std::string, std::string> outcomes; // measureId → expected
};

static std::vector<Expected> buildExpected() {
    return {
        // P001: 40F, pap 2024 → CCS compliant; age 40 < 46 → COL N/A
        {"P001", 40, 'F', {{"CCS","Compliant"}, {"BCS","N/A"},       {"COL","N/A"},       {"CBP","N/A"},       {"CDC","N/A"}}},
        // P002: 55F, pap+HPV 2023 → CCS; mammogram 2025 → BCS; age 55 in COL range, no screening
        {"P002", 55, 'F', {{"CCS","Compliant"}, {"BCS","Compliant"}, {"COL","Non-Compl"}, {"CBP","N/A"},       {"CDC","N/A"}}},
        // P003: 50M, FOBT 2026 → COL compliant; diabetic HbA1c=6.8 → CDC compliant
        {"P003", 50, 'M', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","Compliant"}, {"CBP","N/A"},       {"CDC","Compliant"}}},
        // P004: 60F, hysterectomy → CCS excluded; mammogram 2026 → BCS; age 60 in COL range, no screening
        {"P004", 60, 'F', {{"CCS","Excluded"},  {"BCS","Compliant"}, {"COL","Non-Compl"}, {"CBP","N/A"},       {"CDC","N/A"}}},
        // P005: 70M, colonoscopy 2018 → COL compliant; hypertensive, BP 128/78 → CBP compliant
        {"P005", 70, 'M', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","Compliant"}, {"CBP","Compliant"}, {"CDC","N/A"}}},
        // P006: 30F, no pap → CCS non-compliant; age 30 < 46 → COL N/A
        {"P006", 30, 'F', {{"CCS","Non-Compl"}, {"BCS","N/A"},       {"COL","N/A"},       {"CBP","N/A"},       {"CDC","N/A"}}},
        // P007: 52M, age 52 in COL range, no screening; diabetic HbA1c=9.5 → CDC non-compliant
        {"P007", 52, 'M', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","Non-Compl"}, {"CBP","N/A"},       {"CDC","Non-Compl"}}},
        // P008: 45M, hypertensive, BP 155/95 → CBP non-compliant; age 45 < 46 → COL N/A
        {"P008", 45, 'M', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","N/A"},       {"CBP","Non-Compl"}, {"CDC","N/A"}}},
        // P009: 22F, too young for all measures
        {"P009", 22, 'F', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","N/A"},       {"CBP","N/A"},       {"CDC","N/A"}}},
        // P010: 80M, age 80 > 75 → COL N/A; hypertensive, BP 132/82 → CBP compliant
        {"P010", 80, 'M', {{"CCS","N/A"},       {"BCS","N/A"},       {"COL","N/A"},       {"CBP","Compliant"}, {"CDC","N/A"}}},
    };
}

// Classify the result into a human-readable outcome
static std::string classify(const hedis::MeasureResult& r) {
    if (!r.initialPopulation) return "N/A";
    if (r.denominatorExclusion) return "Excluded";
    if (!r.denominator) return "N/A";
    if (r.numerator) return "Compliant";
    return "Non-Compl";
}

int main(int argc, char* argv[]) {
    std::string walletDir = "wallet";
    std::string tnsService = "hediscql_tp";
    std::string dbUser = "ADMIN";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            walletDir = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            tnsService = argv[++i];
        else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc)
            dbUser = argv[++i];
    }

    std::cout << "=== HEDIS CQL End-to-End Test ===\n\n";

    // --- Connect to Oracle ---
    OCI_Connection* conn = nullptr;
#ifdef HAS_OCILIB
    const char* password = std::getenv("HEDIS_DB_PASSWORD");
    if (!password || !password[0]) {
        std::cerr << "Error: HEDIS_DB_PASSWORD not set\n";
        return 1;
    }
    if (!std::getenv("TNS_ADMIN"))
        setenv("TNS_ADMIN", walletDir.c_str(), 1);

    if (!OCI_Initialize(nullptr, nullptr, OCI_ENV_DEFAULT)) {
        std::cerr << "Failed to initialize OCILIB\n";
        return 1;
    }

    conn = OCI_ConnectionCreate(tnsService.c_str(), dbUser.c_str(),
                                 password, OCI_SESSION_DEFAULT);
    if (!conn) {
        OCI_Error* err = OCI_GetLastError();
        std::cerr << "Oracle connect failed: "
                  << (err ? OCI_ErrorGetString(err) : "unknown") << "\n";
        OCI_Cleanup();
        return 1;
    }
    std::cout << "Connected to Oracle "
              << OCI_GetServerMajorVersion(conn) << "."
              << OCI_GetServerMinorVersion(conn) << "."
              << OCI_GetServerRevisionVersion(conn) << "\n\n";
#else
    std::cerr << "Error: OCILIB not available (built in stub mode)\n";
    return 1;
#endif

    // --- Initialize measure engine ---
    hedis::MeasureEngine engine("config/hedis_cql.ini");
    engine.initialize(conn, "2026");
    std::cout << "Loaded " << engine.measureCount() << " measures from Oracle\n";

    std::cout << "Value sets: " << engine.valueSetManager().valueSetCount()
              << " sets, " << engine.valueSetManager().totalCodeCount() << " codes\n";

    // Fallback to filesystem if Oracle load returned 0 measures or 0 value sets
    if (engine.measureCount() == 0 || engine.valueSetManager().totalCodeCount() == 0) {
        std::cout << "Falling back to filesystem measures/valuesets...\n";
        engine.initializeFromFiles("measures/", "valuesets/hedis_2026_valuesets.json", "2026");
        std::cout << "Loaded " << engine.measureCount() << " measures, "
                  << engine.valueSetManager().valueSetCount() << " sets, "
                  << engine.valueSetManager().totalCodeCount() << " codes from files\n";
    }
    std::cout << "\n";

    // --- Load patients and evaluate ---
    hedis::PatientLoader loader(conn);
    loader.loadPartition(1, 10);  // ROWNUM_ID 1..10

    auto expected = buildExpected();
    std::vector<std::string> measureOrder = {"CCS", "BCS", "COL", "CBP", "CDC"};

    // Store results for comparison
    struct PatientResults {
        std::string patientId;
        std::map<std::string, std::string> outcomes;
    };
    std::vector<PatientResults> allResults;

    hedis::HAPatient patient;
    std::vector<hedis::HAClaim> claims;
    std::vector<hedis::HACoverage> coverages;
    std::vector<hedis::HADiagnosis> diagnoses;
    std::vector<hedis::HAProcedure> procedures;
    std::vector<hedis::HADrug> drugs;
    std::vector<hedis::HALabResult> labs;

    while (loader.loadNextPatient(patient, claims, coverages,
                                   diagnoses, procedures, drugs, labs)) {
        std::cout << "Patient " << patient.patientId
                  << " (" << patient.gender << ", born "
                  << patient.birthDate.year << "-"
                  << std::setfill('0') << std::setw(2) << patient.birthDate.month << "-"
                  << std::setw(2) << patient.birthDate.day << ")"
                  << " — " << coverages.size() << " cov, "
                  << diagnoses.size() << " dx, "
                  << procedures.size() << " px, "
                  << labs.size() << " lab\n";

        auto results = engine.evaluatePatient(patient, claims, coverages,
                                               diagnoses, procedures, drugs, labs);

        PatientResults pr;
        pr.patientId = patient.patientId;
        for (const auto& r : results) {
            std::string outcome = classify(r);
            pr.outcomes[r.measureId] = outcome;
            std::cout << "  " << std::left << std::setw(4) << r.measureId
                      << " IP=" << r.initialPopulation
                      << " DN=" << r.denominator
                      << " NR=" << r.numerator
                      << " EX=" << r.denominatorExclusion
                      << "  → " << outcome << "\n";
        }
        allResults.push_back(std::move(pr));
    }

    // --- Print comparison table ---
    std::cout << "\n=== Results vs Expected ===\n\n";
    std::cout << std::left
              << std::setw(8)  << "Patient"
              << std::setw(12) << "CCS"
              << std::setw(12) << "BCS"
              << std::setw(12) << "COL"
              << std::setw(12) << "CBP"
              << std::setw(12) << "CDC"
              << "\n";
    std::cout << std::string(68, '-') << "\n";

    int pass = 0, fail = 0;
    for (size_t i = 0; i < allResults.size() && i < expected.size(); ++i) {
        const auto& act = allResults[i];
        const auto& exp = expected[i];

        std::cout << std::setw(8) << act.patientId;
        for (const auto& mid : measureOrder) {
            auto actIt = act.outcomes.find(mid);
            auto expIt = exp.outcomes.find(mid);
            std::string actVal = (actIt != act.outcomes.end()) ? actIt->second : "???";
            std::string expVal = (expIt != exp.outcomes.end()) ? expIt->second : "???";

            bool match = (actVal == expVal);
            if (match) {
                std::cout << std::setw(12) << actVal;
                ++pass;
            } else {
                std::cout << std::setw(12) << (actVal + "!" );
                ++fail;
            }
        }
        std::cout << "\n";
    }

    std::cout << "\n" << pass << " passed, " << fail << " failed out of "
              << (pass + fail) << " checks\n";

    if (fail > 0) {
        std::cout << "\nFailed checks (actual != expected):\n";
        for (size_t i = 0; i < allResults.size() && i < expected.size(); ++i) {
            for (const auto& mid : measureOrder) {
                auto actIt = allResults[i].outcomes.find(mid);
                auto expIt = expected[i].outcomes.find(mid);
                std::string actVal = (actIt != allResults[i].outcomes.end()) ? actIt->second : "???";
                std::string expVal = (expIt != expected[i].outcomes.end()) ? expIt->second : "???";
                if (actVal != expVal) {
                    std::cout << "  " << allResults[i].patientId << "/" << mid
                              << ": got " << actVal << ", expected " << expVal << "\n";
                }
            }
        }
    }

#ifdef HAS_OCILIB
    OCI_ConnectionFree(conn);
    OCI_Cleanup();
#endif

    return fail > 0 ? 1 : 0;
}
