// hedis_cql_merge — Merge staging → production, compute rates
// Usage: hedis_cql_merge -s oracle_host -j JOB001

#include "distributed/ResultAggregator.h"
#include <cstring>
#include <iostream>
#include <string>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace {

struct MergeArgs {
    std::string oracleHost = "localhost";
    std::string jobId      = "";
    bool skipBackup        = false;
};

MergeArgs parseArgs(int argc, char* argv[]) {
    MergeArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            args.oracleHost = argv[++i];
        else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc)
            args.jobId = argv[++i];
        else if (strcmp(argv[i], "--skip-backup") == 0)
            args.skipBackup = true;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: hedis_cql_merge [options]\n"
                      << "  -s host          Oracle host\n"
                      << "  -j id            Job ID\n"
                      << "  --skip-backup    Skip production table backup\n";
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

    // Connect to Oracle
    OCI_Connection* conn = nullptr;
#ifdef HAS_OCILIB
    OCI_Initialize(nullptr, nullptr, OCI_ENV_DEFAULT);
    conn = OCI_ConnectionCreate(args.oracleHost.c_str(),
                                 "hedis_app", nullptr, OCI_SESSION_DEFAULT);
    if (!conn) {
        std::cerr << "Failed to connect to Oracle\n";
        return 1;
    }
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
