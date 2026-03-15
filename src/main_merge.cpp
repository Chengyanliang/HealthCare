// hedis_cql_merge — Merge staging → production, compute rates
// Usage: hedis_cql_merge -j JOB001
//
// Environment variables:
//   HEDIS_DB_PASSWORD  - Database password (required for Oracle)
//   TNS_ADMIN          - Wallet directory (default: ./wallet)

#include "distributed/ResultAggregator.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace {

struct MergeArgs {
    std::string tnsService = "hediscql_tp";
    std::string dbUser     = "ADMIN";
    std::string jobId      = "";
    bool skipBackup        = false;
    std::string walletDir  = "wallet";
};

MergeArgs parseArgs(int argc, char* argv[]) {
    MergeArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            args.tnsService = argv[++i];
        else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc)
            args.dbUser = argv[++i];
        else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc)
            args.jobId = argv[++i];
        else if (strcmp(argv[i], "--skip-backup") == 0)
            args.skipBackup = true;
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            args.walletDir = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: hedis_cql_merge [options]\n"
                      << "  -s name          TNS service name (default: hediscql_tp)\n"
                      << "  -u user          Database user (default: ADMIN)\n"
                      << "  -j id            Job ID\n"
                      << "  --skip-backup    Skip production table backup\n"
                      << "  -w dir           Wallet directory (default: wallet)\n"
                      << "\nEnvironment:\n"
                      << "  HEDIS_DB_PASSWORD  Database password\n"
                      << "  TNS_ADMIN          Wallet directory (overrides -w)\n";
            exit(0);
        }
    }
    return args;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);

    if (args.jobId.empty()) {
        std::cerr << "Error: -j JOB_ID is required\n";
        return 1;
    }

    std::cout << "hedis_cql_merge — Staging → Production Merge\n"
              << "  Job ID: " << args.jobId << "\n";

    // Connect to Oracle via wallet
    OCI_Connection* conn = nullptr;
#ifdef HAS_OCILIB
    const char* password = std::getenv("HEDIS_DB_PASSWORD");
    if (!password || !password[0]) {
        std::cerr << "Error: HEDIS_DB_PASSWORD environment variable not set\n";
        return 1;
    }

    if (!std::getenv("TNS_ADMIN")) {
        setenv("TNS_ADMIN", args.walletDir.c_str(), 1);
    }

    if (!OCI_Initialize(nullptr, nullptr, OCI_ENV_DEFAULT | OCI_ENV_THREADED)) {
        std::cerr << "Failed to initialize OCILIB\n";
        return 1;
    }

    conn = OCI_ConnectionCreate(args.tnsService.c_str(),
                                 args.dbUser.c_str(), password,
                                 OCI_SESSION_DEFAULT);
    if (!conn) {
        OCI_Error* err = OCI_GetLastError();
        std::cerr << "Failed to connect to Oracle: "
                  << (err ? OCI_ErrorGetString(err) : "unknown error") << "\n";
        OCI_Cleanup();
        return 1;
    }
    std::cout << "Connected to Oracle "
              << OCI_GetServerMajorVersion(conn) << "."
              << OCI_GetServerMinorVersion(conn) << "."
              << OCI_GetServerRevisionVersion(conn) << "\n";
#else
    std::cout << "Built without OCILIB — dry run mode\n";
#endif

    hedis::ResultAggregator aggregator(conn);

    // Step 1: Verify all partitions are complete
    std::cout << "Verifying all partitions complete...\n";
    if (!aggregator.verifyAllComplete(args.jobId)) {
        std::cerr << "ERROR: Not all partitions are complete for job "
                  << args.jobId << "\n";
        return 1;
    }
    std::cout << "  All partitions complete.\n";

    // Step 2: Backup production table
    if (!args.skipBackup) {
        std::cout << "Backing up production table...\n";
        aggregator.backupProduction(args.jobId);
        std::cout << "  Backup created.\n";
    }

    // Step 3: Merge staging → production
    std::cout << "Merging staging to production...\n";
    aggregator.mergeToProduction(args.jobId);
    std::cout << "  Merge complete.\n";

    // Step 4: Compute rates
    std::cout << "Computing measure rates...\n";
    aggregator.computeRates(args.jobId);
    std::cout << "  Rates computed.\n";

    std::cout << "Merge complete for job " << args.jobId << "\n";

#ifdef HAS_OCILIB
    if (conn) OCI_ConnectionFree(conn);
    OCI_Cleanup();
#endif

    return 0;
}
