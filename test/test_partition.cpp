// test_partition.cpp — Partition management tests
// Included by test_main.cpp (do not compile separately)

#include "distributed/PartitionManager.h"

TEST(partition_info_defaults) {
    hedis::PartitionInfo info;
    ASSERT_EQ(info.partitionId, 0);
    ASSERT_EQ(info.startId, 0);
    ASSERT_EQ(info.endId, 0);
    ASSERT_TRUE(info.partitionName.empty());
    return true;
}

TEST(partition_stats_defaults) {
    hedis::PartitionStats stats;
    ASSERT_EQ(stats.totalPatients, 0);
    ASSERT_EQ(stats.patientsWithCoverage, 0);
    ASSERT_EQ(stats.patientsErrored, 0);
    ASSERT_EQ(stats.alertsGenerated, 0);
    ASSERT_TRUE(stats.elapsedSeconds == 0.0);
    return true;
}

TEST(partition_manager_no_connection) {
    // Without Oracle, claimNextPartition should return false
    hedis::PartitionManager mgr(nullptr);
    hedis::PartitionInfo info;
    ASSERT_FALSE(mgr.claimNextPartition(info));
    return true;
}

TEST(partition_manager_hostname) {
    hedis::PartitionManager mgr(nullptr);
    // Hostname should be non-empty
    ASSERT_FALSE(mgr.hostname().empty());
    return true;
}
