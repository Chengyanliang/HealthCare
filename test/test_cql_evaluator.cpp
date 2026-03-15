// test_cql_evaluator.cpp — CQL evaluator unit tests
// Included by test_main.cpp (do not compile separately)

#include "cql/CQLEvaluator.h"
#include "cql/CQLContext.h"
#include "cql/CQLValue.h"
#include "measure/ValueSetManager.h"
#include "data/DataAdapter.h"

TEST(value_null_semantics) {
    hedis::CQLValue n = hedis::CQLValue::null();
    ASSERT_TRUE(n.isNull());
    ASSERT_FALSE(n.isTruthy());

    // null AND false = false
    auto result = n.cqlAnd(hedis::CQLValue::fromBool(false));
    ASSERT_TRUE(result.isBool());
    ASSERT_FALSE(result.asBool());

    // null AND true = null
    result = n.cqlAnd(hedis::CQLValue::fromBool(true));
    ASSERT_TRUE(result.isNull());

    // null OR true = true
    result = n.cqlOr(hedis::CQLValue::fromBool(true));
    ASSERT_TRUE(result.isBool());
    ASSERT_TRUE(result.asBool());

    // null OR false = null
    result = n.cqlOr(hedis::CQLValue::fromBool(false));
    ASSERT_TRUE(result.isNull());

    return true;
}

TEST(value_date_arithmetic) {
    hedis::CQLDate d = {2026, 1, 15};

    auto d2 = d.addYears(1);
    ASSERT_EQ(d2.year, 2027);
    ASSERT_EQ(d2.month, 1);
    ASSERT_EQ(d2.day, 15);

    auto d3 = d.addMonths(-2);
    ASSERT_EQ(d3.year, 2025);
    ASSERT_EQ(d3.month, 11);
    ASSERT_EQ(d3.day, 15);

    auto d4 = d.addDays(20);
    ASSERT_EQ(d4.year, 2026);
    ASSERT_EQ(d4.month, 2);
    ASSERT_EQ(d4.day, 4);

    int diff = hedis::CQLDate::daysBetween({2026, 1, 1}, {2026, 12, 31});
    ASSERT_EQ(diff, 364);

    return true;
}

TEST(value_interval) {
    auto interval = hedis::CQLValue::fromInterval(
        hedis::CQLValue::fromInt(10),
        hedis::CQLValue::fromInt(20));

    ASSERT_TRUE(interval.inInterval(hedis::CQLValue::fromInt(10)));
    ASSERT_TRUE(interval.inInterval(hedis::CQLValue::fromInt(15)));
    ASSERT_TRUE(interval.inInterval(hedis::CQLValue::fromInt(20)));
    ASSERT_FALSE(interval.inInterval(hedis::CQLValue::fromInt(9)));
    ASSERT_FALSE(interval.inInterval(hedis::CQLValue::fromInt(21)));

    return true;
}

TEST(value_list_operations) {
    hedis::CQLList items;
    items.push_back(hedis::CQLValue::fromInt(1));
    items.push_back(hedis::CQLValue::fromInt(2));
    items.push_back(hedis::CQLValue::fromInt(3));
    auto list = hedis::CQLValue::fromList(items);

    auto contains = list.cqlContains(hedis::CQLValue::fromInt(2));
    ASSERT_TRUE(contains.isBool());
    ASSERT_TRUE(contains.asBool());

    contains = list.cqlContains(hedis::CQLValue::fromInt(5));
    ASSERT_TRUE(contains.isBool());
    ASSERT_FALSE(contains.asBool());

    return true;
}

TEST(evaluator_age_calculation) {
    hedis::ValueSetManager vsMgr;
    hedis::CQLEvaluator evaluator(vsMgr);
    hedis::CQLMeasureParser parser;
    hedis::CQLContext ctx;

    // Create a 45-year-old female patient
    auto patient = hedis::DataAdapter::toPatient(
        "PAT001", "female", 1981, 6, 15);
    ctx.setPatient(patient);

    // Set measurement period to 2026
    hedis::CQLDate mpStart = {2026, 1, 1};
    hedis::CQLDate mpEnd   = {2026, 12, 31};
    ctx.setParameter("Measurement Period",
        hedis::CQLValue::fromInterval(
            hedis::CQLValue::fromDate(mpStart),
            hedis::CQLValue::fromDate(mpEnd)));

    // Use public API: parse a CQL snippet that calls AgeInYearsAt
    auto ast = parser.parse(
        "library AgeTest version '1.0'\n"
        "parameter \"Measurement Period\" Interval<Date>\n"
        "define \"PatientAge\":\n"
        "  AgeInYearsAt(end of \"Measurement Period\")\n"
    );
    ASSERT_TRUE(ast != nullptr);

    auto age = evaluator.evaluateDefine(ast.get(), "PatientAge", ctx);
    ASSERT_TRUE(age.type() == hedis::CQLType::Integer);
    ASSERT_EQ(age.asInt(), 45);

    return true;
}

TEST(evaluator_simple_define) {
    hedis::CQLMeasureParser parser;
    hedis::ValueSetManager vsMgr;
    hedis::CQLEvaluator evaluator(vsMgr);
    hedis::CQLContext ctx;

    auto ast = parser.parse(
        "library Test version '1.0'\n"
        "define \"Always True\": true\n"
        "define \"Always False\": false\n"
    );
    ASSERT_TRUE(ast != nullptr);

    auto results = evaluator.evaluateLibrary(ast.get(), ctx);
    ASSERT_TRUE(results["Always True"].isTruthy());
    ASSERT_FALSE(results["Always False"].isTruthy());

    return true;
}

TEST(context_variable_scoping) {
    hedis::CQLContext ctx;

    ctx.pushScope();
    ctx.setVariable("x", hedis::CQLValue::fromInt(42));
    ASSERT_EQ(ctx.getVariable("x").asInt(), 42);

    ctx.pushScope();
    ctx.setVariable("x", hedis::CQLValue::fromInt(99));
    ASSERT_EQ(ctx.getVariable("x").asInt(), 99);

    ctx.popScope();
    ASSERT_EQ(ctx.getVariable("x").asInt(), 42);

    ctx.popScope();
    ASSERT_TRUE(ctx.getVariable("x").isNull());

    return true;
}
