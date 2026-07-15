#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <cassert>

#include "Protocol.h"
#include "CellularCore.h"

int main() {
    try {
        std::cout << "Running concurrency tests...\n";
        std::shared_ptr<Protocol> proto = std::make_shared<OFDMProtocol>();
        CellularCore core(proto);
        core.calculate_max_devices(1.0);

        const int threads = 8;
        const int perThread = 1000;
        std::vector<std::thread> ths;
        for (int t = 0; t < threads; ++t) {
            ths.emplace_back([&core]() {
                for (int i = 0; i < perThread; ++i) {
                    core.generate_messages(1);
                }
            });
        }
        for (auto &t : ths) t.join();

        long long total = core.get_total_messages_generated();
        std::cout << "Expected: " << (long long)threads * perThread << " got: " << total << "\n";
        assert(total == (long long)threads * perThread);
        std::cout << "Concurrency test passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Concurrency test failed: " << e.what() << "\n";
        return 2;
    }
}
