// test_measures.cpp — End-to-end measure evaluation tests
// Included by test_main.cpp (do not compile separately)

#include "cql/CQLEvaluator.h"
#include "cql/CQLParser.h"
#include "measure/MeasureDefinition.h"
#include "measure/ValueSetManager.h"
#include "data/DataAdapter.h"

TEST(measure_ccs_eligible_compliant) {
    // Scenario: 40-year-old female with recent pap smear → compliant
    hedis::ValueSetManager vsMgr;
    vsMgr.loadFromFile("valuesets/hedis_2026_valuesets.json");

    hedis::CQLMeasureParser parser;
    auto ast = parser.parseFile("measures/CCS_CervicalCancerScreening.cql");
    if (!ast) {
        std::cout << "(skipped — measure files not in CWD) ";
        return true;
    }

    hedis::MeasureDefinition measure;
    measure.measureId = "CCS";
    measure.version = "2026";
    measure.ast = std::move(ast);
    measure.resolvePopulations();

    // Create patient
    hedis::CQLContext ctx;
    auto patient = hedis::DataAdapter::toPatient("P001", "female", 1986, 3, 15);
    ctx.setPatient(patient);

    // Add coverage
    std::vector<hedis::HACoverage> coverages = {
        hedis::DataAdapter::toCoverage("medical", 2026, 1, 1, 2026, 12, 31)
    };
    ctx.setCoverages(coverages);

    // Add a pap smear procedure in 2025
    std::vector<hedis::HAProcedure> procedures = {
        hedis::DataAdapter::toProcedure("88175", "CPT", 2025, 6, 10)
    };
    ctx.setProcedures(procedures);

    // Set measurement period
    ctx.setParameter("Measurement Period",
        hedis::CQLValue::fromInterval(
            hedis::CQLValue::fromDate({2026, 1, 1}),
            hedis::CQLValue::fromDate({2026, 12, 31})));

    hedis::CQLEvaluator evaluator(vsMgr);
    hedis::MeasureResult result = measure.evaluate(evaluator, ctx);

    ASSERT_TRUE(result.initialPopulation);  // 40-year-old female
    ASSERT_TRUE(result.denominator);         // Has coverage
    ASSERT_TRUE(result.numerator);           // Has pap smear within 3 years
    ASSERT_FALSE(result.denominatorExclusion);

    return true;
}

TEST(measure_ccs_male_excluded) {
    // Males should not be in initial population for CCS
    hedis::ValueSetManager vsMgr;
    hedis::CQLMeasureParser parser;
    auto ast = parser.parseFile("measures/CCS_CervicalCancerScreening.cql");
    if (!ast) {
        std::cout << "(skipped) ";
        return true;
    }

    hedis::MeasureDefinition measure;
    measure.measureId = "CCS";
    measure.ast = std::move(ast);
    measure.resolvePopulations();

    hedis::CQLContext ctx;
    auto patient = hedis::DataAdapter::toPatient("P002", "male", 1986, 3, 15);
    ctx.setPatient(patient);
    ctx.setParameter("Measurement Period",
        hedis::CQLValue::fromInterval(
            hedis::CQLValue::fromDate({2026, 1, 1}),
            hedis::CQLValue::fromDate({2026, 12, 31})));

    hedis::CQLEvaluator evaluator(vsMgr);
    hedis::MeasureResult result = measure.evaluate(evaluator, ctx);

    ASSERT_FALSE(result.initialPopulation);
    ASSERT_FALSE(result.denominator);
    ASSERT_FALSE(result.numerator);

    return true;
}

TEST(measure_definition_resolve_populations) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse(
        "library Test version '1.0'\n"
        "define \"Initial Population\": true\n"
        "define \"Denominator\": true\n"
        "define \"Numerator\": false\n"
        "define \"Denominator Exclusion\": false\n"
    );
    ASSERT_TRUE(ast != nullptr);

    hedis::MeasureDefinition def;
    def.ast = std::move(ast);
    def.resolvePopulations();

    ASSERT_EQ(def.populations.size(), 4u);

    // Verify population names match
    bool hasIP = false, hasDen = false, hasNum = false, hasExcl = false;
    for (const auto& pop : def.populations) {
        if (pop.name == "Initial Population")    hasIP = true;
        if (pop.name == "Denominator")           hasDen = true;
        if (pop.name == "Numerator")             hasNum = true;
        if (pop.name == "Denominator Exclusion") hasExcl = true;
    }
    ASSERT_TRUE(hasIP);
    ASSERT_TRUE(hasDen);
    ASSERT_TRUE(hasNum);
    ASSERT_TRUE(hasExcl);

    return true;
}

TEST(valueset_manager_load_and_lookup) {
    hedis::ValueSetManager vsMgr;
    vsMgr.loadFromFile("valuesets/hedis_2026_valuesets.json");

    if (vsMgr.valueSetCount() == 0) {
        std::cout << "(skipped — JSON not in CWD) ";
        return true;
    }

    // Should find a CPT code in Mammography
    ASSERT_TRUE(vsMgr.isMember("Mammography", "77067", "CPT"));
    ASSERT_FALSE(vsMgr.isMember("Mammography", "99999", "CPT"));

    // Should find LOINC code in HbA1c
    ASSERT_TRUE(vsMgr.isMember("HbA1c Lab Test", "4548-4", "LOINC"));

    return true;
}
