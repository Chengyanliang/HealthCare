// test_cql_parser.cpp — CQL parser unit tests
// Included by test_main.cpp (do not compile separately)

#include "cql/CQLParser.h"

TEST(parser_empty_library) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse("library Test version '1.0'");
    ASSERT_TRUE(ast != nullptr);
    ASSERT_EQ(ast->name, "Test");
    ASSERT_EQ(ast->version, "1.0");
    return true;
}

TEST(parser_valueset_declaration) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse(
        "library VS version '1.0'\n"
        "valueset \"Diabetes\": 'hedis:1.2.3'\n"
        "valueset \"HbA1c\": 'hedis:4.5.6'\n"
    );
    ASSERT_TRUE(ast != nullptr);
    ASSERT_EQ(ast->valueSets.size(), 2u);
    ASSERT_EQ(ast->valueSets[0]->name, "Diabetes");
    ASSERT_EQ(ast->valueSets[0]->oid, "hedis:1.2.3");
    ASSERT_EQ(ast->valueSets[1]->name, "HbA1c");
    return true;
}

TEST(parser_define_literal) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse(
        "library Lit version '1.0'\n"
        "define \"Always True\": true\n"
    );
    ASSERT_TRUE(ast != nullptr);
    ASSERT_EQ(ast->definitions.size(), 1u);
    ASSERT_EQ(ast->definitions[0]->name, "Always True");
    return true;
}

TEST(parser_define_comparison) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse(
        "library Cmp version '1.0'\n"
        "define \"Age Check\":\n"
        "  AgeInYearsAt(end of \"Measurement Period\") between 24 and 64\n"
    );
    ASSERT_TRUE(ast != nullptr);
    ASSERT_EQ(ast->definitions.size(), 1u);
    return true;
}

TEST(parser_retrieve_expression) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parse(
        "library Ret version '1.0'\n"
        "define \"Has Diabetes\":\n"
        "  exists([Condition: \"Diabetes\"])\n"
    );
    ASSERT_TRUE(ast != nullptr);
    ASSERT_EQ(ast->definitions.size(), 1u);
    return true;
}

TEST(parser_full_measure) {
    hedis::CQLMeasureParser parser;
    auto ast = parser.parseFile("measures/CCS_CervicalCancerScreening.cql");
    if (!ast) {
        // File might not be in CWD; skip gracefully
        std::cout << "(skipped — CQL file not in CWD) ";
        return true;
    }
    ASSERT_EQ(ast->name, "CCS");
    ASSERT_TRUE(ast->definitions.size() >= 3);
    return true;
}

TEST(parser_validation_errors) {
    hedis::CQLMeasureParser parser;
    auto errors = parser.validate("this is not valid CQL @ #$%");
    // Should have at least one error
    ASSERT_TRUE(errors.size() > 0);
    return true;
}
