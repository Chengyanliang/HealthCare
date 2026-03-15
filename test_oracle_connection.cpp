// test_oracle_connection — Test OCILIB connection to Oracle Cloud Autonomous Database
// Usage: test_oracle_connection
//
// Environment variables:
//   HEDIS_DB_PASSWORD       - ADMIN password (required)
//   TNS_ADMIN               - Wallet directory (default: ./wallet)

#include <cstdlib>
#include <iostream>
#include <string>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

int main() {
#ifndef HAS_OCILIB
    std::cerr << "Not compiled with Oracle support (HAS_OCILIB not defined)\n";
    return 1;
#else
    // Get password from environment
    const char* password = std::getenv("HEDIS_DB_PASSWORD");
    if (!password || !password[0]) {
        std::cerr << "Set HEDIS_DB_PASSWORD environment variable first\n";
        return 1;
    }

    // Wallet directory — OCILIB uses TNS_ADMIN to find tnsnames.ora / sqlnet.ora
    const char* tnsAdmin = std::getenv("TNS_ADMIN");
    std::string walletDir = tnsAdmin ? tnsAdmin : "wallet";
    std::cout << "Wallet dir: " << walletDir << "\n";

    // Set TNS_ADMIN if not already set (OCILIB/OCI needs this)
    if (!tnsAdmin) {
        setenv("TNS_ADMIN", walletDir.c_str(), 1);
        std::cout << "Set TNS_ADMIN=" << walletDir << "\n";
    }

    // Initialize OCILIB
    std::cout << "Initializing OCILIB...\n";
    if (!OCI_Initialize(nullptr, nullptr, OCI_ENV_DEFAULT | OCI_ENV_THREADED)) {
        std::cerr << "OCI_Initialize failed\n";
        return 1;
    }

    // Connect using TNS service name from tnsnames.ora
    const char* tnsService = "hediscql_tp";
    const char* username   = "ADMIN";

    std::cout << "Connecting to " << tnsService << " as " << username << "...\n";
    OCI_Connection* conn = OCI_ConnectionCreate(tnsService, username, password,
                                                 OCI_SESSION_DEFAULT);
    if (!conn) {
        OCI_Error* err = OCI_GetLastError();
        if (err) {
            std::cerr << "Connection failed: " << OCI_ErrorGetString(err) << "\n";
        } else {
            std::cerr << "Connection failed (no error details)\n";
        }
        OCI_Cleanup();
        return 1;
    }

    std::cout << "Connected! Server version: "
              << OCI_GetServerMajorVersion(conn) << "."
              << OCI_GetServerMinorVersion(conn) << "."
              << OCI_GetServerRevisionVersion(conn) << "\n";

    // Test 1: Basic query
    std::cout << "\n--- Test 1: SYSDATE ---\n";
    OCI_Statement* stmt = OCI_StatementCreate(conn);
    OCI_ExecuteStmt(stmt, "SELECT SYSDATE, USER FROM DUAL");
    OCI_Resultset* rs = OCI_GetResultset(stmt);
    if (OCI_FetchNext(rs)) {
        std::cout << "  Server time: " << OCI_GetString(rs, 1) << "\n";
        std::cout << "  User:        " << OCI_GetString(rs, 2) << "\n";
    }
    OCI_StatementFree(stmt);

    // Test 2: Check HEDIS tables
    std::cout << "\n--- Test 2: HEDIS schema tables ---\n";
    stmt = OCI_StatementCreate(conn);
    OCI_ExecuteStmt(stmt,
        "SELECT TABLE_NAME FROM USER_TABLES "
        "WHERE TABLE_NAME LIKE 'SMA_CQL%' ORDER BY TABLE_NAME");
    rs = OCI_GetResultset(stmt);
    int tableCount = 0;
    while (OCI_FetchNext(rs)) {
        std::cout << "  " << OCI_GetString(rs, 1) << "\n";
        ++tableCount;
    }
    std::cout << "  Total: " << tableCount << " tables\n";
    OCI_StatementFree(stmt);

    // Test 3: Count measures
    std::cout << "\n--- Test 3: CQL measures ---\n";
    stmt = OCI_StatementCreate(conn);
    OCI_ExecuteStmt(stmt,
        "SELECT MEASURE_ID, LENGTH(CQL_TEXT) FROM SMA_CQL_MEASURES "
        "WHERE STATUS = 'A' ORDER BY MEASURE_ID");
    rs = OCI_GetResultset(stmt);
    while (OCI_FetchNext(rs)) {
        std::cout << "  " << OCI_GetString(rs, 1)
                  << " (" << OCI_GetInt(rs, 2) << " bytes)\n";
    }
    OCI_StatementFree(stmt);

    // Test 4: Count value sets
    std::cout << "\n--- Test 4: Value sets ---\n";
    stmt = OCI_StatementCreate(conn);
    OCI_ExecuteStmt(stmt,
        "SELECT VALUESET_NAME, COUNT(*) FROM SMA_CQL_VALUESETS "
        "GROUP BY VALUESET_NAME ORDER BY VALUESET_NAME");
    rs = OCI_GetResultset(stmt);
    int totalCodes = 0;
    while (OCI_FetchNext(rs)) {
        int cnt = OCI_GetInt(rs, 2);
        std::cout << "  " << OCI_GetString(rs, 1) << ": " << cnt << " codes\n";
        totalCodes += cnt;
    }
    std::cout << "  Total: " << totalCodes << " codes\n";
    OCI_StatementFree(stmt);

    // Cleanup
    OCI_ConnectionFree(conn);
    OCI_Cleanup();

    std::cout << "\nOracle Cloud connection test PASSED.\n";
    return 0;
#endif
}
