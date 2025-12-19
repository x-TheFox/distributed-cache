#include <iostream>
#include <chrono>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include "sharder/consistent_hash.h"
#include "sharder/rebalancer.h"
#include "replication/mock_replicator.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    const int NODES = 12;
    const int KEYS = 200000; // large to measure time

    std::unordered_map<std::string, std::shared_ptr<MockReplicator>> nodes;
    ConsistentHash ring(100);
    for (int i = 0; i < NODES; ++i) {
        std::string id = "b" + std::to_string(i);
        nodes[id] = std::make_shared<MockReplicator>(MockReplicatorOptions{});
        ring.add_node(id);
    }

    // seed
    for (int i = 0; i < KEYS; ++i) {
        std::string k = "bk-" + std::to_string(i);
        std::string v = "bv-" + std::to_string(i);
        auto owner = ring.get_node(k);
        nodes[owner]->replicate(k, v);
    }

    // add new node and time rebalancer migration
    auto new_id = std::string("b_new");
    nodes[new_id] = std::make_shared<MockReplicator>(MockReplicatorOptions{});
    ring.add_node(new_id);
    Rebalancer rebalancer(ring, nodes);

    auto start = std::chrono::steady_clock::now();
    size_t moved = rebalancer.execute_migrations_for_add(new_id);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    std::cout << "Rebalanced " << moved << " keys in " << ms << " ms\n";

    // write a simple CSV result for CI artifact collection (portable implementation)
    try {
        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm buf{};
        localtime_r(&now_t, &buf);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &buf);
        std::string outdir = std::string("benchmarks/results/") + ts;
        std::string cmd = std::string("mkdir -p ") + outdir;
        std::system(cmd.c_str());
        std::string outfile = outdir + "/rebalancer_results.csv";
        std::ofstream ofs(outfile);
        if (ofs) {
            ofs << "timestamp,nodes,keys,moved,ms\n";
            ofs << ts << "," << (NODES+1) << "," << KEYS << "," << moved << "," << ms << "\n";
            ofs.close();
        } else {
            std::cerr << "Failed to open output file: " << outfile << "\n";
        }
    } catch (...) {
        std::cerr << "Failed to write results\n";
    }

    // cleanup
    for (auto &p : nodes) p.second->stop();
    return 0;
}
