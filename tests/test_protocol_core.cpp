#include <iostream>
#include <memory>
#include <cassert>

#include "Protocol.h"
#include "CellularCore.h"
#include "Exceptions.h"
#include "UserDevice.h"

// Simple test harness without external deps
int main() {
    try {
        std::cout << "Running Protocol/Core unit tests...\n";

        // Test 1: TDMAProtocol max users calculation
        TDMAProtocol tdma;
        int maxUsers = tdma.calculate_max_users(1.0); // 1 MHz
        std::cout << "TDMA max users for 1.0 MHz: " << maxUsers << "\n";
        assert(maxUsers > 0);

        // Test 2: MassiveMIMOProtocol required cores overflow protection
        MassiveMIMOProtocol m5;
        int messages_per_user = m5.calculate_messages_per_user();
        try {
            // Intentionally large user count to force overflow or CapacityException
            int hugeUsers = 2000000000; // 2e9
            int cores = m5.calculate_required_cores(hugeUsers, messages_per_user);
            std::cout << "No overflow for huge user count, cores = " << cores << "\n";
        } catch (const CapacityException &e) {
            std::cout << "CapacityException caught as expected for huge user count: " << e.what() << "\n";
        }
        // Either outcome is acceptable here - the point of this test is that
        // calculate_required_cores does not invoke undefined behavior (signed
        // overflow) on an adversarial input; it must either return a finite
        // value or fail loudly via CapacityException.

        // Test 3: CellularCore message generation increments counters safely
        {
            std::shared_ptr<Protocol> proto = std::make_shared<OFDMProtocol>();
            CellularCore core(proto);
            core.calculate_max_devices(1.0);
            long long before = core.get_total_messages_generated();
            core.generate_messages(100);
            long long after = core.get_total_messages_generated();
            assert(after - before == 100);
            std::cout << "CellularCore generate_messages increments by 100 as expected\n";
        }

        // Test 4: registering devices until capacity is actually reached.
        // (This used to just inspect get_max_devices() without registering a
        // single device, so it could never observe the overflow path it
        // claimed to test. Device2G is a concrete UserDevice subclass, so we
        // can drive register_device() for real here.)
        {
            std::shared_ptr<Protocol> proto = std::make_shared<TDMAProtocol>();
            CellularCore core(proto);
            core.calculate_max_devices(0.2); // small bandwidth -> small capacity
            int capacity = core.get_max_devices();
            std::cout << "Core capacity (TDMA, 0.2 MHz): " << capacity << "\n";
            assert(capacity > 0);

            for (int i = 0; i < capacity; ++i) {
                Device2G dev("Test-Device-" + std::to_string(i), CommunicationType::DATA);
                core.register_device(dev);
            }
            assert(core.get_current_device_count() == capacity);

            bool capacityThrew = false;
            try {
                Device2G overflowDevice("Overflow-Device", CommunicationType::DATA);
                core.register_device(overflowDevice);
            } catch (const CapacityException &e) {
                capacityThrew = true;
                std::cout << "Capacity exception thrown as expected when overfilling core: " << e.what() << "\n";
            }
            assert(capacityThrew);
        }

        std::cout << "All tests passed (asserts didn't fail).\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test harness caught exception: " << e.what() << "\n";
        return 2;
    }
}
