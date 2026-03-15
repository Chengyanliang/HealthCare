// hedis_cql — Worker process (runs on each of 24 servers)
// Usage: hedis_cql -s oracle_host -t 8 -c config/hedis_cql.ini

#include "engine/MeasureEngine.h"
#include "engine/WorkerPool.h"
#include "data/PatientLoader.h"
#include "distributed/PartitionManager.h"
#include "distributed/StagingWriter.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

// #include <ocilib.h>

namespace {

struct WorkerArgs {
    std::string oracleHost  = "localhost";
    int         threadCount = 8;
    std::string configFile  = "config/hedis_cql.ini";
    std::string measuresDir = "measures/";
    std::string valueSetsFile = "valuesets/hedis_2026_valuesets.json";
    std::string measurementYear = "2026";
};

WorkerArgs parseArgs(int argc, char* argv[]) {
    WorkerArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            args.oracleHost = argv[++i];
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
            args.threadCount = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            args.configFile = argv[++i];
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            args.measuresDir = argv[++i];
        else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc)
            args.valueSetsFile = argv[++i];
        else if (strcmp(argv[i], "-y") == 0 && i + 1 < argc)
            args.measurementYear = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: hedis_cql [options]\n"
                      << "  -s host    Oracle host\n"
                      << "  -t N       Thread count (default: 8)\n"
                      << "  -c file    Config file\n"
                      << "  -m dir     Measures directory\n"
                      << "  -v file    Value sets JSON file\n"
                      << "  -y year    Measurement year (default: 2026)\n";
            exit(0);
        }
    }
    return args;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);

    std::cout << "hedis_cql worker starting\n"
              << "  Oracle host: " << args.oracleHost << "\n"
              << "  Threads:     " << args.threadCount << "\n"
              << "  Config:      " << args.configFile << "\n";

    // Connect to Oracle
    OCI_Connection* conn = nullptr;
#ifdef HAS_OCILIB
    OCI_Initialize(nullptr, nullptr, OCI_ENV_DEFAULT);
    conn = OCI_ConnectionCreate(args.oracleHost.c_str(),
                                 "hedis_app", nullptr, OCI_SESSION_DEFAULT);
    if (!conn) {
        std::cerr << "Failed to connect to Oracle: " << args.oracleHost << "\n";
        return 1;
    }
#endif

    // Initialize engine
    hedis::MeasureEngine engine(args.configFile);
    if (conn) {
        engine.initialize(conn, args.measurementYear);
    } else {
        std::cout << "No Oracle connection — loading from filesystem\n";
        engine.initializeFromFiles(args.measuresDir, args.valueSetsFile,
                                    args.measurementYear);
    }
    std::cout << "Loaded " << engine.measureCount() << " measures\n";

    // Claim and process partitions
    hedis::PartitionManager partMgr(conn);
    hedis::WorkerPool pool(args.threadCount, engine);
    hedis::PatientLoader loader(conn);

    int partitionsProcessed = 0;
    hedis::PartitionInfo partition;

    while (partMgr.claimNextPartition(partition)) {
        std::cout << "Processing partition " << partition.partitionId
                  << " (patients " << partition.startId
                  << " - " << partition.endId << ")\n";

        auto startTime = std::chrono::steady_clock::now();

        hedis::StagingWriter writer(conn, partition.partitionId);
        pool.processPartition(partition, loader, writer);

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        double secs = std::chrono::duration<double>(elapsed).count();

        hedis::PartitionStats stats;
        stats.totalPatients       = pool.processedCount();
        stats.patientsErrored     = pool.errorCount();
        stats.elapsedSeconds      = secs;
        stats.alertsGenerated     = writer.writtenCount();

        partMgr.completePartition(partition.partitionId, stats);

        std::cout << "Partition " << partition.partitionId << " complete: "
                  << stats.totalPatients << " patients, "
                  << stats.alertsGenerated << " results, "
                  << secs << "s\n";

        ++partitionsProcessed;
    }

    std::cout << "Worker finished: " << partitionsProcessed
              << " partitions processed\n";

#ifdef HAS_OCILIB
    if (conn) OCI_ConnectionFree(conn);
    OCI_Cleanup();
#endif

    return 0;
}
