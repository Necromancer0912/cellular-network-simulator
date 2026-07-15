#include "NetworkAnalytics.h"
#include "Exceptions.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

#include "IOHelpers.h"

// NetworkAnalytics implementation
NetworkAnalytics::NetworkAnalytics(const Simulator *sim) : simulator(sim) {}

NetworkAnalytics::~NetworkAnalytics() {}

PerformanceMetrics NetworkAnalytics::calculate_current_metrics() const {
  PerformanceMetrics metrics;
  metrics.total_devices = simulator->get_device_count();
  metrics.connected_devices = 0;
  metrics.total_messages = 0;

  double totalUtil = 0.0;
  metrics.peak_utilization = 0.0;

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (tower) {
      double util = tower->get_utilization();
      totalUtil += util;
      if (util > metrics.peak_utilization) {
        metrics.peak_utilization = util;
      }
      metrics.connected_devices += tower->get_current_device_count();
    }
  }

  metrics.average_utilization = simulator->get_tower_count() > 0
                                    ? totalUtil / simulator->get_tower_count()
                                    : 0.0;
  metrics.failed_connections =
      metrics.total_devices - metrics.connected_devices;
  metrics.success_rate =
      metrics.total_devices > 0
          ? (100.0 * metrics.connected_devices / metrics.total_devices)
          : 0.0;
  metrics.messages_per_core = 0.0; // Will be calculated based on total messages

  return metrics;
}

void NetworkAnalytics::display_performance_report() const {
  
  
  
  
  
  
  

  OutputFormatter::print_header("NETWORK PERFORMANCE REPORT");

  auto metrics = calculate_current_metrics();

  // Network Overview
  IOHelpers::print("Network Overview"), IOHelpers::printNewline();
  std::vector<std::pair<std::string, std::string>> overviewData = {
      {"Total Towers", std::to_string(simulator->get_tower_count())},
      {"Total Devices", std::to_string(metrics.total_devices)},
      {"Connected Devices", std::to_string(metrics.connected_devices)},
      {"Failed Connections", std::to_string(metrics.failed_connections)}};
  OutputFormatter::print_summary_table(overviewData);
  IOHelpers::printNewline();

  // Performance Metrics
  IOHelpers::print("Performance Metrics"), IOHelpers::printNewline();
  std::stringstream ss_success;
  ss_success << std::fixed << std::setprecision(2) << metrics.success_rate
             << "%";
  std::stringstream ss_avg_util;
  ss_avg_util << std::fixed << std::setprecision(2)
              << metrics.average_utilization << "%";
  std::stringstream ss_peak_util;
  ss_peak_util << std::fixed << std::setprecision(2) << metrics.peak_utilization
               << "%";

  std::vector<std::pair<std::string, std::string>> perfData = {
      {"Connection Success Rate", ss_success.str()},
      {"Average Tower Utilization", ss_avg_util.str()},
      {"Peak Tower Utilization", ss_peak_util.str()}};
  OutputFormatter::print_summary_table(perfData);
  IOHelpers::printNewline();

  // Health status
  IOHelpers::print("Network Health: ");
  if (metrics.average_utilization < 50.0) {
    Logger::success("OPTIMAL");
  } else if (metrics.average_utilization < 75.0) {
    Logger::warning("MODERATE");
  } else {
    Logger::error("CRITICAL");
  }
}

void NetworkAnalytics::display_utilization_graph() const {
  OutputFormatter::print_header("TOWER UTILIZATION GRAPH");

  if (simulator->get_tower_count() == 0) {
    IOHelpers::print("No towers in the network."), IOHelpers::printNewline();
    return;
  }

  IOHelpers::print("Tower | Utilization (%)"), IOHelpers::printNewline();
  IOHelpers::print("------+");
  for (int i = 0; i < 50; i++)
    IOHelpers::print("─");
  IOHelpers::printNewline();

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (tower) {
      double util = tower->get_utilization();
      // Format: "ID   | " (fixed width 5 for ID)
      char idBuf[8];
      IOHelpers::intToString(i, idBuf, 8);
      IOHelpers::print(idBuf);
      
      // Padding for ID
      int idLen = 0;
      while(idBuf[idLen] != '\0') idLen++;
      for(int k=0; k<5-idLen; k++) IOHelpers::print(" ");
      
      IOHelpers::print(" | ");

      int bars = static_cast<int>(util / 2.0);
      std::string barColor = util > 80 ? TerminalColor::BOLD_RED : (util > 50 ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_GREEN);
      IOHelpers::print(barColor.c_str());
      for (int j = 0; j < bars; j++) {
        IOHelpers::print("█");
      }
      for (int j = bars; j < 50; j++) {
        IOHelpers::print("░");
      }
      IOHelpers::print(TerminalColor::RESET.c_str());
      IOHelpers::print(" ");
      IOHelpers::printDouble(util, 1);
      IOHelpers::print("%");
      IOHelpers::printNewline();
    }
  }
}

void NetworkAnalytics::compare_all_generations() const {
  OutputFormatter::print_header("GENERATION COMPARISON");
  IOHelpers::printNewline();
  IOHelpers::print("  Comparing all cellular generations (per 1 MHz bandwidth)");
  IOHelpers::printNewline();
  IOHelpers::printNewline();

  std::vector<std::string> generations = {"2G", "3G", "4G", "5G", "6G", "7G"};
  double bandwidth = 1.0; // 1 MHz

  // Table headers with proper spacing
  std::vector<std::string> headers = {"Generation", "Max Users", "Msgs/User", "Cores", "Efficiency"};
  std::vector<int> widths = {12, 12, 12, 10, 12};
  OutputFormatter::print_table_header(headers, widths);

  for (const auto &gen : generations) {
    auto protocol = simulator->get_protocol(gen);
    if (protocol) {
      int max_users = protocol->calculate_max_users(bandwidth);
      int msgs = protocol->calculate_messages_per_user();
      int cores = protocol->calculate_required_cores(max_users, msgs);
      double efficiency = cores > 0 ? static_cast<double>(max_users) / cores : 0.0;

      std::vector<std::string> row = {
        gen,
        std::to_string(max_users),
        std::to_string(msgs),
        std::to_string(cores),
        std::to_string((int)efficiency) + " users/core"
      };
      OutputFormatter::print_table_row(row, widths);
    }
  }
  OutputFormatter::print_table_separator(widths);
  IOHelpers::printNewline();
  IOHelpers::print("  Note: Higher efficiency means better resource utilization");
  IOHelpers::printNewline();
}

void NetworkAnalytics::suggest_optimal_configuration(
    const std::string &generation) const {
  OutputFormatter::print_header("OPTIMAL CONFIGURATION SUGGESTION");

  auto protocol = simulator->get_protocol(generation);
  if (!protocol) {
    Logger::error("Protocol not found");
    return;
  }

  IOHelpers::print(" Generation: "), IOHelpers::print(generation.c_str()), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Calculate for various bandwidth scenarios
  std::vector<double> bandwidths = {1.0, 5.0, 10.0, 20.0};

  IOHelpers::print(" Recommended Configurations"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  for (double bw : bandwidths) {
    int max_users = protocol->calculate_max_users(bw);
    int msgs = protocol->calculate_messages_per_user();
    int cores = protocol->calculate_required_cores(max_users, msgs);

    IOHelpers::print(" "), IOHelpers::print("▸ Bandwidth: "), IOHelpers::printDouble(bw, 1), IOHelpers::print(" MHz"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("  Max Users:      "), IOHelpers::printInt(max_users), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("  Required Cores: "), IOHelpers::printInt(cores), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("  Users per Core: "), IOHelpers::printInt((cores > 0 ? max_users / cores : 0)), IOHelpers::printNewline();
    IOHelpers::printNewline();
  }
}

void NetworkAnalytics::analyze_bottlenecks() const {
  OutputFormatter::print_header("BOTTLENECK ANALYSIS");

  if (simulator->get_tower_count() == 0) {
    IOHelpers::print("No towers to analyze."), IOHelpers::printNewline();
    return;
  }

  std::vector<std::pair<int, double>> towerUtils;
  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (tower) {
      towerUtils.push_back({i, tower->get_utilization()});
    }
  }

  // Sort by utilization (descending)
  std::sort(towerUtils.begin(), towerUtils.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  IOHelpers::print("All Towers (Sorted by Utilization):"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  for (const auto &tu : towerUtils) {
    auto tower = simulator->get_tower(tu.first);
    IOHelpers::print("Tower #"), IOHelpers::printInt(tu.first), IOHelpers::print(": "), IOHelpers::println(tower->get_location().c_str());
    IOHelpers::print(" Utilization:       "), IOHelpers::printDouble(tu.second, 1), IOHelpers::print("% "), IOHelpers::printNewline();
    IOHelpers::print(" Connected Devices: "), IOHelpers::printInt(tower->get_current_device_count()), IOHelpers::printNewline();
    IOHelpers::print(" Max Capacity:      "), IOHelpers::printInt(tower->get_max_supported_devices()), IOHelpers::printNewline();

    if (tu.second > 90.0) {
      Logger::error("  Status: CRITICAL - Add more towers or cores");
    } else if (tu.second > 75.0) {
      Logger::warning("  Status: HIGH - Consider load balancing");
    } else {
      Logger::success("  Status: NORMAL");
    }
    IOHelpers::printNewline();
  }
}

void NetworkAnalytics::display_network_topology() const {
  OutputFormatter::print_header("NETWORK TOPOLOGY VISUALIZATION");

  if (simulator->get_tower_count() == 0) {
    IOHelpers::print(" "), IOHelpers::print("No towers in the network."), IOHelpers::printNewline();
    return;
  }

  IOHelpers::print(" Total Towers: "), IOHelpers::printInt(simulator->get_tower_count()), IOHelpers::printNewline();
  IOHelpers::printNewline();

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (!tower)
      continue;

    double util = tower->get_utilization();
    std::string utilColor = (util < 50) ? TerminalColor::BOLD_GREEN : ((util < 80) ? TerminalColor::BOLD_YELLOW : TerminalColor::BOLD_RED);

    // Minimalist Tree Style
    IOHelpers::print(" ├── Tower #");
    IOHelpers::printInt(i);
    IOHelpers::print(" [");
    IOHelpers::print(tower->get_location().c_str());
    IOHelpers::print("]\n");
    
    IOHelpers::print(" │   ├── Protocol:    ");
    IOHelpers::println(tower->get_protocol()->get_protocol_name().c_str());
    
    IOHelpers::print(" │   ├── Cores:       ");
    IOHelpers::printInt(tower->get_number_of_cores());
    IOHelpers::printNewline();
    
    IOHelpers::print(" │   ├── Capacity:    ");
    IOHelpers::printInt(tower->get_current_device_count());
    IOHelpers::print("/");
    IOHelpers::printInt(tower->get_max_supported_devices());
    IOHelpers::printNewline();
    
    IOHelpers::print(" │   ├── Utilization: ");
    IOHelpers::print(utilColor.c_str());
    IOHelpers::printDouble(util, 1);
    IOHelpers::print("%");
    IOHelpers::print(TerminalColor::RESET.c_str());
    IOHelpers::printNewline();

    int deviceCount = std::min(tower->get_current_device_count(), 5);
    if (deviceCount > 0) {
      IOHelpers::print(" │   └── Connected Devices (showing first ");
      IOHelpers::printInt(deviceCount);
      IOHelpers::print("):\n");
      for (int j = 0; j < deviceCount; j++) {
        IOHelpers::print(" │       ├── Device #");
        // Get the device's actual name/id if we want, or just show index
        IOHelpers::printInt(j);
        IOHelpers::printNewline();
      }
      if (tower->get_current_device_count() > 5) {
        IOHelpers::print(" │       └── ... +");
        IOHelpers::printInt(tower->get_current_device_count() - 5);
        IOHelpers::println(" more");
      }
    } else {
      IOHelpers::print(" │   └── No connected devices\n");
    }
    IOHelpers::printNewline();
  }
}

// LoadBalancer implementation
LoadBalancer::LoadBalancer(Simulator *sim) : simulator(sim) {}

LoadBalancer::~LoadBalancer() {}

void LoadBalancer::set_device_qos(int device_id, QoSLevel level) {
  device_qos[device_id] = level;
}

QoSLevel LoadBalancer::get_device_qos(int device_id) const {
  auto it = device_qos.find(device_id);
  return (it != device_qos.end()) ? it->second : QoSLevel::MEDIUM;
}

QoSLevel LoadBalancer::effective_qos(const std::shared_ptr<UserDevice> &device) const {
  if (!device) return QoSLevel::MEDIUM;
  auto it = device_qos.find(device->get_device_id());
  if (it != device_qos.end()) return it->second;

  switch (device->get_communication_type()) {
    case CommunicationType::VOICE: return QoSLevel::HIGH;
    case CommunicationType::BOTH:  return QoSLevel::MEDIUM;
    case CommunicationType::DATA:  return QoSLevel::LOW;
    default: return QoSLevel::MEDIUM;
  }
}

int LoadBalancer::find_best_tower(QoSLevel qos) const {
  int best = -1;
  double bestScore = -1.0;

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (!tower) continue;
    if (tower->get_current_device_count() >= tower->get_max_supported_devices()) continue; // full

    double util = tower->get_utilization();
    double score;
    if (qos == QoSLevel::CRITICAL || qos == QoSLevel::HIGH) {
      // High-priority traffic goes wherever has the most headroom.
      score = 100.0 - util;
    } else {
      // Lower-priority traffic fills towers already in reasonable use
      // (target ~70%) instead of consuming the best-available tower,
      // leaving headroom on lightly loaded towers for HIGH/CRITICAL
      // devices that connect later.
      if (util > 85.0) continue;
      score = 100.0 - std::abs(70.0 - util);
    }
    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }
  return best;
}

void LoadBalancer::redistribute_devices() {
  const double OVERLOAD_THRESHOLD = 85.0;
  const double MIN_IMPROVEMENT = 15.0; // only hand over if meaningfully better off

  int moved = 0;
  for (int t = 0; t < simulator->get_tower_count(); ++t) {
    auto tower = simulator->get_tower(t);
    if (!tower || tower->get_utilization() < OVERLOAD_THRESHOLD) continue;

    // Snapshot the device list before mutating the tower mid-iteration.
    auto devicesOnTower = tower->get_all_devices();
    for (auto &device : devicesOnTower) {
      if (!device) continue;
      QoSLevel qos = effective_qos(device);
      if (qos == QoSLevel::CRITICAL) continue; // never migrate critical sessions

      int targetIdx = find_best_tower(qos);
      if (targetIdx < 0 || targetIdx == t) continue;

      auto target = simulator->get_tower(targetIdx);
      if (!target) continue;
      if (tower->get_utilization() - target->get_utilization() < MIN_IMPROVEMENT) continue;

      int deviceId = device->get_device_id();
      if (target->connect_device(device)) {
        tower->disconnect_device(deviceId);
        moved++;
        Logger::info("Load balancer moved device " + std::to_string(deviceId) +
                     " from Tower #" + std::to_string(t) + " to Tower #" + std::to_string(targetIdx));
      }
    }
  }

  if (moved > 0) {
    Logger::success("Load balancer redistributed " + std::to_string(moved) + " device(s) off overloaded towers.");
  } else {
    Logger::info("Load balancer found no overloaded towers to redistribute.");
  }
}

void LoadBalancer::balance_load() {
  double peakBefore = 0.0;
  for (int i = 0; i < simulator->get_tower_count(); ++i) {
    auto tower = simulator->get_tower(i);
    if (tower) peakBefore = std::max(peakBefore, tower->get_utilization());
  }

  redistribute_devices();

  double peakAfter = 0.0;
  for (int i = 0; i < simulator->get_tower_count(); ++i) {
    auto tower = simulator->get_tower(i);
    if (tower) peakAfter = std::max(peakAfter, tower->get_utilization());
  }

  std::stringstream ss;
  ss << "Peak tower utilization: " << std::fixed << std::setprecision(1)
     << peakBefore << "% -> " << peakAfter << "%";
  Logger::info(ss.str());
}

int LoadBalancer::find_least_loaded_tower() const {
  int bestTower = -1;
  double minUtil = 100.0;

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (tower) {
      double util = tower->get_utilization();
      if (util < minUtil) {
        minUtil = util;
        bestTower = i;
      }
    }
  }
  return bestTower;
}

int LoadBalancer::find_most_loaded_tower() const {
  int worstTower = -1;
  double maxUtil = 0.0;

  for (int i = 0; i < simulator->get_tower_count(); i++) {
    auto tower = simulator->get_tower(i);
    if (tower) {
      double util = tower->get_utilization();
      if (util > maxUtil) {
        maxUtil = util;
        worstTower = i;
      }
    }
  }
  return worstTower;
}

void LoadBalancer::display_qos_statistics() const {
  OutputFormatter::print_header("QUALITY OF SERVICE STATISTICS");

  std::map<QoSLevel, int> qosCount;
  qosCount[QoSLevel::LOW] = 0;
  qosCount[QoSLevel::MEDIUM] = 0;
  qosCount[QoSLevel::HIGH] = 0;
  qosCount[QoSLevel::CRITICAL] = 0;

  for (const auto &pair : device_qos) {
    qosCount[pair.second]++;
  }

  IOHelpers::print("QoS Distribution:"), IOHelpers::printNewline();
  IOHelpers::print(" Low Priority: "), IOHelpers::print(qosCount[QoSLevel::LOW]), IOHelpers::printNewline();
  IOHelpers::print(" Medium Priority: "), IOHelpers::print(qosCount[QoSLevel::MEDIUM]), IOHelpers::printNewline();
  IOHelpers::print(" High Priority: "), IOHelpers::print(qosCount[QoSLevel::HIGH]), IOHelpers::printNewline();
  IOHelpers::print(" Critical Priority: "), IOHelpers::print(qosCount[QoSLevel::CRITICAL]), IOHelpers::printNewline();
}

// HandoverManager implementation
HandoverManager::HandoverManager(Simulator *sim) : simulator(sim) {}

HandoverManager::~HandoverManager() {}

bool HandoverManager::perform_handover(int deviceIndex, int from_tower,
                                       int to_tower) {
  std::shared_ptr<UserDevice> targetDevice = nullptr;
  for (int i = 0; i < simulator->get_device_count(); ++i) {
    auto dev = simulator->get_device(i);
    if (dev && dev->get_device_id() == deviceIndex) {
      targetDevice = dev;
      break;
    }
  }

  auto fromT = simulator->get_tower(from_tower);
  auto toT = simulator->get_tower(to_tower);

  bool connected = false;
  if (toT && targetDevice) {
    connected = toT->connect_device(targetDevice);
  }

  if (connected && fromT) {
    fromT->disconnect_device(deviceIndex);
  }

  HandoverEvent event;
  event.device_id = deviceIndex;
  event.from_tower = from_tower;
  event.to_tower = to_tower;
  event.timestamp = "NOW";
  event.success = connected;

  handover_history.push_back(event);
  return event.success;
}

void HandoverManager::simulate_random_handovers(int numHandovers) {
  if (simulator->get_tower_count() < 2 || simulator->get_device_count() == 0) return;
  
  int count = 0;
  for (int attempt = 0; attempt < numHandovers * 3 && count < numHandovers; ++attempt) {
    int dev_idx = rand() % simulator->get_device_count();
    auto device = simulator->get_device(dev_idx);
    if (!device || !device->get_connection_status()) continue;
    
    int current_tower_idx = -1;
    for (int t = 0; t < simulator->get_tower_count(); ++t) {
      auto tower = simulator->get_tower(t);
      if (tower && tower->get_device(device->get_device_id())) {
        current_tower_idx = t;
        break;
      }
    }
    
    if (current_tower_idx == -1) continue;
    
    int target_tower_idx = rand() % simulator->get_tower_count();
    if (target_tower_idx == current_tower_idx) {
      target_tower_idx = (target_tower_idx + 1) % simulator->get_tower_count();
    }
    
    if (perform_handover(device->get_device_id(), current_tower_idx, target_tower_idx)) {
      count++;
    }
  }
}

void HandoverManager::display_handover_history() const {
  OutputFormatter::print_header("HANDOVER HISTORY");

  if (handover_history.empty()) {
    IOHelpers::print("No handovers recorded."), IOHelpers::printNewline();
    return;
  }

  IOHelpers::print("Device"), IOHelpers::print("From Tower"), IOHelpers::print("To Tower"), IOHelpers::print("Status"), IOHelpers::printNewline();
  IOHelpers::print(std::string(45, '-')), IOHelpers::printNewline();

  for (const auto &event : handover_history) {
    IOHelpers::print(event.device_id), IOHelpers::print("        "), IOHelpers::print(event.from_tower), IOHelpers::print("          "), IOHelpers::print(event.to_tower), IOHelpers::print("        "), IOHelpers::print((event.success ? "SUCCESS" : "FAILED")), IOHelpers::printNewline();
  }

  IOHelpers::printNewline();
  IOHelpers::print("Total Handovers: "), IOHelpers::print((int)handover_history.size()), IOHelpers::printNewline();
  IOHelpers::print("Success Rate: "), IOHelpers::print(get_handover_success_rate()), IOHelpers::print("%"), IOHelpers::printNewline();
}

double HandoverManager::get_handover_success_rate() const {
  if (handover_history.empty())
    return 0.0;

  int successful = 0;
  for (const auto &event : handover_history) {
    if (event.success)
      successful++;
  }
  return 100.0 * successful / handover_history.size();
}

// ScenarioManager implementation
void ScenarioManager::load_scenario(Simulator &sim, ScenarioType type) {
  sim.reset();

  switch (type) {
  case ScenarioType::URBAN_DENSE:
    Logger::info("Loading Urban Dense scenario...");
    sim.create_tower("5G", "Downtown-North", 20.0, 15);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 300.0);
    sim.create_tower("5G", "Downtown-South", 20.0, 15);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 700.0);
    sim.create_tower("4G", "Business-District", 10.0, 10);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(300.0, 500.0);
    sim.create_tower("4G", "Shopping-Mall", 10.0, 12);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(700.0, 500.0);

    for (int i = 0; i < 4; ++i) {
      try {
        sim.create_and_connect_device("5G", "City-N-" + std::to_string(i), CommunicationType::BOTH, 0);
        sim.create_and_connect_device("5G", "City-S-" + std::to_string(i), CommunicationType::BOTH, 1);
        sim.create_and_connect_device("4G", "Office-" + std::to_string(i), CommunicationType::DATA, 2);
        sim.create_and_connect_device("4G", "Mall-" + std::to_string(i), CommunicationType::BOTH, 3);
      } catch (...) {}
    }
    Logger::success("Urban Dense scenario loaded");
    break;

  case ScenarioType::SUBURBAN:
    Logger::info("Loading Suburban scenario...");
    sim.create_tower("4G", "Residential-Area-1", 5.0, 5);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(300.0, 300.0);
    sim.create_tower("4G", "Residential-Area-2", 5.0, 5);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(700.0, 700.0);
    sim.create_tower("3G", "Community-Center", 2.0, 3);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 500.0);

    for (int i = 0; i < 3; ++i) {
      try {
        sim.create_and_connect_device("4G", "Sub-1-" + std::to_string(i), CommunicationType::BOTH, 0);
        sim.create_and_connect_device("4G", "Sub-2-" + std::to_string(i), CommunicationType::VOICE, 1);
        sim.create_and_connect_device("3G", "Comm-" + std::to_string(i), CommunicationType::BOTH, 2);
      } catch (...) {}
    }
    Logger::success("Suburban scenario loaded");
    break;

  case ScenarioType::RURAL:
    Logger::info("Loading Rural scenario...");
    sim.create_tower("3G", "Village-Central", 1.0, 2);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 500.0);
    sim.create_tower("2G", "Farmland-North", 1.0, 1);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(200.0, 200.0);
    sim.create_tower("2G", "Farmland-South", 1.0, 1);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(800.0, 800.0);

    for (int i = 0; i < 4; ++i) {
      try {
        sim.create_and_connect_device("3G", "Villager-" + std::to_string(i), CommunicationType::BOTH, 0);
        sim.create_and_connect_device("2G", "Farmer-N-" + std::to_string(i), CommunicationType::VOICE, 1);
        sim.create_and_connect_device("2G", "Farmer-S-" + std::to_string(i), CommunicationType::VOICE, 2);
      } catch (...) {}
    }
    Logger::success("Rural scenario loaded");
    break;

  case ScenarioType::HIGHWAY:
    Logger::info("Loading Highway scenario...");
    sim.create_tower("4G", "Highway-Mile-10", 5.0, 5);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(200.0, 500.0);
    sim.create_tower("4G", "Highway-Mile-20", 5.0, 5);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 500.0);
    sim.create_tower("4G", "Highway-Mile-30", 5.0, 5);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(800.0, 500.0);
    sim.create_tower("3G", "Rest-Area", 2.0, 3);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 300.0);

    for (int i = 0; i < 6; ++i) {
      try {
        sim.create_and_connect_device("4G", "Car-A-" + std::to_string(i), CommunicationType::DATA, 0);
        sim.create_and_connect_device("4G", "Car-B-" + std::to_string(i), CommunicationType::DATA, 1);
        sim.create_and_connect_device("4G", "Car-C-" + std::to_string(i), CommunicationType::DATA, 2);
        sim.create_and_connect_device("3G", "Rest-User-" + std::to_string(i), CommunicationType::BOTH, 3);
      } catch (...) {}
    }
    Logger::success("Highway scenario loaded");
    break;

  case ScenarioType::STADIUM:
    Logger::info("Loading Stadium scenario...");
    sim.create_tower("5G", "Stadium-Section-A", 20.0, 20);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(400.0, 400.0);
    sim.create_tower("5G", "Stadium-Section-B", 20.0, 20);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(600.0, 400.0);
    sim.create_tower("5G", "Stadium-Section-C", 20.0, 20);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(400.0, 600.0);
    sim.create_tower("5G", "Stadium-Section-D", 20.0, 20);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(600.0, 600.0);

    for (int i = 0; i < 6; ++i) {
      try {
        sim.create_and_connect_device("5G", "Fan-A-" + std::to_string(i), CommunicationType::BOTH, 0);
        sim.create_and_connect_device("5G", "Fan-B-" + std::to_string(i), CommunicationType::BOTH, 1);
        sim.create_and_connect_device("5G", "Fan-C-" + std::to_string(i), CommunicationType::BOTH, 2);
        sim.create_and_connect_device("5G", "Fan-D-" + std::to_string(i), CommunicationType::BOTH, 3);
      } catch (...) {}
    }
    Logger::success("Stadium scenario loaded");
    break;

  case ScenarioType::DISASTER_RECOVERY:
    Logger::info("Loading Disaster Recovery scenario...");
    sim.create_tower("7G", "Mobile-Unit-1", 5.0, 10);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(450.0, 450.0);
    sim.create_tower("6G", "Mobile-Unit-2", 5.0, 8);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(550.0, 450.0);
    sim.create_tower("5G", "Emergency-Base", 10.0, 12);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(500.0, 550.0);

    for (int i = 0; i < 8; ++i) {
      try {
        sim.create_and_connect_device("7G", "Responder-1-" + std::to_string(i), CommunicationType::BOTH, 0);
        sim.create_and_connect_device("6G", "Responder-2-" + std::to_string(i), CommunicationType::BOTH, 1);
        sim.create_and_connect_device("5G", "Base-Staff-" + std::to_string(i), CommunicationType::BOTH, 2);
      } catch (...) {}
    }
    Logger::success("Disaster Recovery scenario loaded");
    break;

  case ScenarioType::MIXED_GENERATION:
    Logger::info("Loading Mixed Generation scenario...");
    sim.create_tower("2G", "Legacy-Zone", 1.0, 2);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(150.0, 150.0);
    sim.create_tower("3G", "Transition-Zone", 2.0, 4);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(350.0, 350.0);
    sim.create_tower("4G", "Modern-Zone-1", 5.0, 8);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(550.0, 550.0);
    sim.create_tower("5G", "Modern-Zone-2", 10.0, 12);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(750.0, 750.0);
    sim.create_tower("6G", "Future-Zone", 15.0, 10);
    sim.get_tower(sim.get_tower_count() - 1)->set_position(900.0, 900.0);

    for (int i = 0; i < 5; ++i) {
      try {
        sim.create_and_connect_device("2G", "Legacy-Dev-" + std::to_string(i), CommunicationType::VOICE, 0);
        sim.create_and_connect_device("3G", "Transit-Dev-" + std::to_string(i), CommunicationType::BOTH, 1);
        sim.create_and_connect_device("4G", "Modern-1-Dev-" + std::to_string(i), CommunicationType::DATA, 2);
        sim.create_and_connect_device("5G", "Modern-2-Dev-" + std::to_string(i), CommunicationType::BOTH, 3);
        sim.create_and_connect_device("6G", "Future-Dev-" + std::to_string(i), CommunicationType::BOTH, 4);
      } catch (...) {}
    }
    Logger::success("Mixed Generation scenario loaded");
    break;
  }
}

void ScenarioManager::display_available_scenarios() {
  OutputFormatter::print_header("AVAILABLE NETWORK SCENARIOS");

  IOHelpers::print("\n");

  // Scenario 1: Urban Dense
  IOHelpers::print(" "), IOHelpers::print("[ 1]"), IOHelpers::print(" "), IOHelpers::print("URBAN DENSE"), IOHelpers::print(" - "), IOHelpers::print("High-density city center with 5G/4G infrastructure"), IOHelpers::print("\n");

  // Scenario 2: Suburban
  IOHelpers::print(" "), IOHelpers::print("[ 2]"), IOHelpers::print(" "), IOHelpers::print("SUBURBAN"), IOHelpers::print(" - "), IOHelpers::print("Residential areas with balanced 4G/3G coverage"), IOHelpers::print("\n");

  // Scenario 3: Rural
  IOHelpers::print(" "), IOHelpers::print("[ 3]"), IOHelpers::print(" "), IOHelpers::print("RURAL"), IOHelpers::print(" - "), IOHelpers::print("Low-density farmland with basic 3G/2G connectivity"), IOHelpers::print("\n");

  // Scenario 4: Highway
  IOHelpers::print(" "), IOHelpers::print("[ 4]"), IOHelpers::print(" "), IOHelpers::print("HIGHWAY"), IOHelpers::print(" - "), IOHelpers::print("Linear deployment along highway corridors"), IOHelpers::print("\n");

  // Scenario 5: Stadium
  IOHelpers::print(" "), IOHelpers::print("[ 5]"), IOHelpers::print(" "), IOHelpers::print("STADIUM"), IOHelpers::print(" - "), IOHelpers::print("High-capacity venue with advanced 5G network"), IOHelpers::print("\n");

  // Scenario 6: Disaster Recovery
  IOHelpers::print(" "), IOHelpers::print("[ 6]"), IOHelpers::print(" "), IOHelpers::print("DISASTER RECOVERY"), IOHelpers::print(" - "), IOHelpers::print("Emergency mobile units with latest technology"), IOHelpers::print("\n");

  // Scenario 7: Mixed Generation
  IOHelpers::print(" "), IOHelpers::print("[ 7]"), IOHelpers::print(" "), IOHelpers::print("MIXED GENERATION"), IOHelpers::print(" - "), IOHelpers::print("Network evolution showcase (2G → 7G)"), IOHelpers::print("\n");

  // Back option
  IOHelpers::print("\n "), IOHelpers::print("[ 0]"), IOHelpers::print(" "), IOHelpers::print("Back to Main Menu"), IOHelpers::print("\n\n");
}

std::string ScenarioManager::get_scenario_description(ScenarioType type) {
  switch (type) {
  case ScenarioType::URBAN_DENSE:
    return "High-density urban area with modern 5G and 4G infrastructure";
  case ScenarioType::SUBURBAN:
    return "Medium-density suburban area with 4G and 3G coverage";
  case ScenarioType::RURAL:
    return "Low-density rural area with basic 3G and 2G connectivity";
  case ScenarioType::HIGHWAY:
    return "Linear highway deployment with 4G coverage";
  case ScenarioType::STADIUM:
    return "High-capacity stadium with advanced 5G infrastructure";
  case ScenarioType::DISASTER_RECOVERY:
    return "Emergency scenario with mobile units and latest technology";
  case ScenarioType::MIXED_GENERATION:
    return "Network evolution showcase with all generations";
  default:
    return "Unknown scenario";
  }
}
