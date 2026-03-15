// hedis_cql — Worker process (runs on each of 24 servers)
// Usage: hedis_cql -t 8 -c config/hedis_cql.ini
//
// Environment variables:
//   HEDIS_DB_PASSWORD  - Database password (required for Oracle)
//   TNS_ADMIN          - Wallet directory (default: ./wallet)

#include "engine/MeasureEngine.h"
#include "engine/WorkerPool.h"
#include "data/PatientLoader.h"
#include "distributed/PartitionManager.h"
#include "distributed/StagingWriter.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef HAS_OCILIB
#include <ocilib.h>
#endif

namespace {

struct WorkerArgs {
    std::string tnsService      = "hediscql_tp";
    std::string dbUser          = "ADMIN";
    int         threadCount     = 8;
    std::string configFile      = "config/hedis_cql.ini";
    std::string measuresDir     = "measures/";
    std::string valueSetsFile   = "valuesets/hedis_2026_valuesets.json";
    std::string measurementYear = "2026";
    std::string walletDir       = "wallet";
};

WorkerArgs parseArgs(int argc, char* argv[]) {
    WorkerArgs args;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            args.tnsService = argv[++i];
        else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc)
            args.dbUser = argv[++i];
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
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            args.walletDir = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: hedis_cql [options]\n"
                      << "  -s name    TNS service name (default: hediscql_tp)\n"
                      << "  -u user    Database user (default: ADMIN)\n"
                      << "  -t N       Thread count (default: 8)\n"
                      << "  -c file    Config file\n"
                      << "  -m dir     Measures directory\n"
                      << "  -v file    Value sets JSON file\n"
                      << "  -y year    Measurement year (default: 2026)\n"
                      << "  -w dir     Wallet directory (default: wallet)\n"
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

    std::cout << "hedis_cql worker starting\n"
              << "  TNS service: " << args.tnsService << "\n"
              << "  User:        " << args.dbUser << "\n"
              << "  Threads:     " << args.threadCount << "\n"
              << "  Config:      " << args.configFile << "\n";

    // Connect to Oracle via wallet
    OCI_Connection* conn = nullptr;
#ifdef HAS_OCILIB
    const char* password = std::getenv("HEDIS_DB_PASSWORD");
    if (!password || !password[0]) {
        std::cerr << "Error: HEDIS_DB_PASSWORD environment variable not set\n";
        return 1;
    }

    // Set TNS_ADMIN for wallet-based TLS connection
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
