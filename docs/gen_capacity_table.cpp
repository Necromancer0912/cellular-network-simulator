#include "Protocol.h"
#include <iostream>
#include <memory>
#include <iomanip>

int main() {
    std::vector<std::pair<std::string, std::shared_ptr<Protocol>>> protocols = {
        {"2G", std::make_shared<TDMAProtocol>()},
        {"3G", std::make_shared<CDMAProtocol>()},
        {"4G", std::make_shared<OFDMProtocol>()},
        {"5G", std::make_shared<MassiveMIMOProtocol>()},
        {"6G", std::make_shared<TerahertzProtocol>()},
        {"7G", std::make_shared<HolographicProtocol>()},
    };
    double bw = 10.0;
    std::cout << std::left << std::setw(6) << "Gen" << std::setw(14) << "MaxUsers"
              << std::setw(12) << "Msgs/User" << std::setw(14) << "ReqCores" << "Efficiency\n";
    for (auto &p : protocols) {
        int maxUsers = p.second->calculate_max_users(bw);
        int msgs = p.second->calculate_messages_per_user();
        int cores = p.second->calculate_required_cores(maxUsers, msgs);
        double eff = (double)maxUsers / cores;
        std::cout << std::left << std::setw(6) << p.first << std::setw(14) << maxUsers
                  << std::setw(12) << msgs << std::setw(14) << cores << std::fixed << std::setprecision(2) << eff << "\n";
    }

    std::cout << "\n--- bandwidth=1.0 MHz (original README basis) ---\n";
    bw = 1.0;
    for (auto &p : protocols) {
        int maxUsers = p.second->calculate_max_users(bw);
        int msgs = p.second->calculate_messages_per_user();
        int cores = p.second->calculate_required_cores(maxUsers, msgs);
        double eff = (double)maxUsers / cores;
        std::cout << std::left << std::setw(6) << p.first << std::setw(14) << maxUsers
                  << std::setw(12) << msgs << std::setw(14) << cores << std::fixed << std::setprecision(2) << eff << "\n";
    }
    return 0;
}
