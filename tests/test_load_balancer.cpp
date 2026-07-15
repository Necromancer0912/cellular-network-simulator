#include <iostream>
#include <cassert>

#include "Simulator.h"
#include "NetworkAnalytics.h"
#include "Exceptions.h"

// LoadBalancer::balance_load/redistribute_devices/find_best_tower were
// declared in NetworkAnalytics.h but had no implementation anywhere in the
// codebase, and LoadBalancer was never instantiated by any reachable code
// path - this test exercises the real implementation end to end.
int main() {
    try {
        std::cout << "Running LoadBalancer tests...\n";

        Simulator sim;

        // A small-capacity tower that will fill up, and a roomy one that
        // should have capacity to absorb the overflow.
        sim.create_tower("4G", "Tiny", 1.0, 1);
        sim.create_tower("4G", "Roomy", 50.0, 40);

        int connected = 0;
        for (int i = 0; i < 200; ++i) {
            try {
                sim.create_and_connect_device("4G", "T" + std::to_string(i),
                                              CommunicationType::DATA, 0);
                connected++;
            } catch (const CellularNetworkException &) {
                break; // Tiny tower is full.
            }
        }
        std::cout << "Connected " << connected << " devices to Tiny before it filled up\n";

        auto tiny = sim.get_tower(0);
        auto roomy = sim.get_tower(1);
        assert(tiny && roomy);

        double tinyUtilBefore = tiny->get_utilization();
        int roomyCountBefore = roomy->get_current_device_count();
        std::cout << "Tiny utilization before balance: " << tinyUtilBefore << "%\n";
        assert(tinyUtilBefore > 85.0); // must actually be overloaded for this test to mean anything

        LoadBalancer lb(&sim);
        lb.redistribute_devices();

        double tinyUtilAfter = tiny->get_utilization();
        int roomyCountAfter = roomy->get_current_device_count();
        std::cout << "Tiny utilization after balance: " << tinyUtilAfter << "%\n";
        std::cout << "Roomy device count: " << roomyCountBefore << " -> " << roomyCountAfter << "\n";

        assert(tinyUtilAfter < tinyUtilBefore); // some devices actually moved
        assert(roomyCountAfter > roomyCountBefore); // and they landed on Roomy

        // find_best_tower should never hand back a full tower.
        int best = lb.find_best_tower(QoSLevel::HIGH);
        assert(best == 1); // only Roomy has headroom left in this scenario

        std::cout << "All LoadBalancer tests passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "LoadBalancer test failed: " << e.what() << "\n";
        return 2;
    }
}
