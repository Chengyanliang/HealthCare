// hedis_cql_ctl — Partition controller (creates partitions before workers start)
// Usage: hedis_cql_ctl -s oracle_host -j JOB001 -n 24 -b 25000 -r 20260101

#include "distributed/PartitionManager.h"
#include "measure/MeasureStore.h"
#include <cstring>
#include <iostream>
#include <string>

// #include <ocilib.h>

namespace {

struct CtlArgs {
    std::string oracleHost    = "localhost";
    std::string jobId         = "";
    int         numPartitions = 24;
    int         batchSize     = 25000;
    int         runYear       = 2026;
    int         runMonth      = 1;
    int         runDay        = 1;
    std::string measuresDir   = "measures/";
};

CtlArgs parseArgs(int argc, char* argv[]) {
    CtlArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            args.oracleHost = argv[++i];
        else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc)
            args.jobId = argv[++i];
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            args.numPartitions = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
            args.batchSize = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            std::string date = argv[++i];
            if (date.size() == 8) {
                args.runYear  = std::stoi(date.substr(0, 4));
                args.runMonth = std::stoi(date.substr(4, 2));
                args.runDay   = std::stoi(date.substr(6, 2));
            }
        }
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            args.measuresDir = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: hedis_cql_ctl [options]\n"
                      << "  -s host    Oracle host\n"
                      << "  -j id      Job ID\n"
                      << "  -n N       Number of partitions (default: 24)\n"
                      << "  -b N       Batch size (default: 25000)\n"
                      << "  -r DATE    Run date YYYYMMDD\n"
                      << "  -m dir     Measures directory for initial load\n";
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

    std::cout << "hedis_cql_ctl — Partition Controller\n"
              << "  Job ID:     " << args.jobId << "\n"
              << "  Partitions: " << args.numPartitions << "\n"
              << "  Batch size: " << args.batchSize << "\n"
              << "  Run date:   " << args.runYear << "-"
              << args.runMonth << "-" << args.runDay << "\n";

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

    // Create partitions
    hedis::PartitionManager partMgr(conn);
    partMgr.createPartitions(args.numPartitions, args.batchSize,
                              args.jobId,
                              args.runYear, args.runMonth, args.runDay);
    std::cout << "Created " << args.numPartitions << " partitions\n";

    // Optionally load CQL measures into database from filesystem
    if (conn) {
        hedis::MeasureStore store(conn);
        auto measures = store.loadFromDirectory(args.measuresDir);
        for (const auto& m : measures) {
            store.saveMeasure(m);
            std::cout << "  Loaded measure: " << m.measureId
                      << " v" << m.version << "\n";
        }
    }

    std::cout << "Partition setup complete. Workers can now start.\n";

#ifdef HAS_OCILIB
    if (conn) OCI_ConnectionFree(conn);
    OCI_Cleanup();
#endif

    return 0;
}
