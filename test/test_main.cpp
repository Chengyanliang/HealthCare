// Minimal test runner — no external testing framework dependency
// Build: scons WITH_TESTS=1
// Run:   ./hedis_cql_tests

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstdlib>

struct TestCase {
    std::string name;
    std::function<bool()> fn;
};

static std::vector<TestCase>& tests() {
    static std::vector<TestCase> t;
    return t;
}

#define TEST(name) \
    static bool test_##name(); \
    static bool _reg_##name = (tests().push_back({#name, test_##name}), true); \
    static bool test_##name()

#define ASSERT_TRUE(expr)  do { if (!(expr)) { std::cerr << "  FAIL: " #expr " at " __FILE__ ":" << __LINE__ << "\n"; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)   do { if ((a) != (b)) { std::cerr << "  FAIL: " #a " != " #b " at " __FILE__ ":" << __LINE__ << "\n"; return false; } } while(0)

// --- Pull in test files ---
#include "test_cql_parser.cpp"
#include "test_cql_evaluator.cpp"
#include "test_measures.cpp"
#include "test_partition.cpp"

int main() {
    int passed = 0, failed = 0;
    for (const auto& tc : tests()) {
        std::cout << "  " << tc.name << " ... ";
        if (tc.fn()) {
            std::cout << "OK\n";
            ++passed;
        } else {
            std::cout << "FAILED\n";
            ++failed;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
