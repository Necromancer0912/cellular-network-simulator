#include <iostream>
#include <memory>
#include <cassert>

#include "Protocol.h"
#include "CellularCore.h"
#include "Exceptions.h"

// Minimal concrete UserDevice for tests
class DummyDevice : public UserDevice {
public:
    DummyDevice(const std::string &name, CommunicationType type, int messages)
        : UserDevice(name, type), msgCount(messages) {}
    virtual int get_message_count() const override { return msgCount; }
    virtual std::string get_device_type() const override { return "Dummy"; }
private:
    int msgCount;
};

int main() {
    try {
        std::cout << "Running exception tests...\n";

        // Test: CellularCore's message counter accumulates correctly across
        // calls. generate_messages() takes an int, so a single call can add
        // at most ~2.1 billion messages; reaching the LLONG_MAX overflow
        // guard in CellularCore::generate_messages would take billions of
        // calls and isn't practical to exercise here. What we can actually
        // verify is that repeated additions land on the exact expected
        // total, with no silent truncation or drift.
        std::shared_ptr<Protocol> proto = std::make_shared<MassiveMIMOProtocol>();
        CellularCore core(proto);
        core.calculate_max_devices(1.0);
        core.generate_messages(100);
        core.generate_messages(1000000000); // 1e9, near the int range but far from LLONG_MAX
        long long afterAccumulate = core.get_total_messages_generated();
        assert(afterAccumulate == 100LL + 1000000000LL);
        std::cout << "Message counter accumulated correctly: " << afterAccumulate << "\n";

        // Test: register_device throws when capacity reached
        TDMAProtocol tdma;
        std::shared_ptr<Protocol> p2 = std::make_shared<TDMAProtocol>();
        CellularCore core2(p2);
        core2.calculate_max_devices(0.2); // small capacity
        int capacity = core2.get_max_devices();
        std::cout << "Core2 capacity: " << capacity << "\n";
    DummyDevice d("test-device", CommunicationType::DATA, 20);
        bool capacityThrew = false;
        try {
            for (int i = 0; i < capacity + 2; ++i) {
                if (!core2.can_accommodate_device(d)) {
                    core2.register_device(d); // should throw once full
                } else {
                    core2.register_device(d);
                }
            }
        } catch (const CapacityException &e) {
            capacityThrew = true;
            std::cout << "Capacity exception thrown as expected when overfilling core: " << e.what() << "\n";
        }

        assert(capacityThrew);
        std::cout << "Exception tests completed (capacityThrew=" << capacityThrew << ")\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test harness exception: " << e.what() << "\n";
        return 2;
    }
}
