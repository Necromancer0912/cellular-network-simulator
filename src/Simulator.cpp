#include "Simulator.h"
#include "AdvancedMetrics.h"
#include "Exceptions.h"
#include "Utils.h"
#include "IOHelpers.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <atomic>
#include <thread>
#include <condition_variable>

Simulator::Simulator()
  : towers("Cell Towers"), devices("User Devices"), threadPool(std::max(1u, std::thread::hardware_concurrency())),
    next_packet_id(1), dropped_packets(0), processed_packets(0), total_packets_routed(0) {
  initialize_protocols();
  obstacles.assign(100, std::vector<bool>(100, false));
  // Pre-place some default buildings (obstacles) in 100x100 space
  for (int r = 30; r <= 50; r++) {
    for (int c = 25; c <= 32; c++) {
      obstacles[r][c] = true;
    }
  }
  for (int r = 60; r <= 65; r++) {
    for (int c = 60; c <= 80; c++) {
      obstacles[r][c] = true;
    }
  }
  Logger::info("Cellular Network Simulator initialized");
}

Simulator::~Simulator() = default;

void Simulator::initialize_protocols() {
  available_protocols["2G"] = std::make_shared<TDMAProtocol>();
  available_protocols["3G"] = std::make_shared<CDMAProtocol>();
  available_protocols["4G"] = std::make_shared<OFDMProtocol>();
  available_protocols["5G"] = std::make_shared<MassiveMIMOProtocol>();
  available_protocols["6G"] = std::make_shared<TerahertzProtocol>();
  available_protocols["7G"] = std::make_shared<HolographicProtocol>();
}

std::shared_ptr<UserDevice>
Simulator::create_device(const std::string &generation, const std::string &name,
                         CommunicationType type) {
  if (generation == "2G") {
    return std::make_shared<Device2G>(name, type);
  } else if (generation == "3G") {
    return std::make_shared<Device3G>(name, type);
  } else if (generation == "4G") {
    return std::make_shared<Device4G>(name, type);
  } else if (generation == "5G") {
    return std::make_shared<Device5G>(name, type);
  } else if (generation == "6G") {
    return std::make_shared<Device6G>(name, type);
  } else if (generation == "7G") {
    return std::make_shared<Device7G>(name, type);
  }
  throw ConfigurationException("Unknown generation: " + generation);
}

void Simulator::create_tower(const std::string &generation,
                             const std::string &location, double bandwidthMHz,
                             int num_cores) {
  try {
    auto protocol = get_protocol(generation);
    if (!protocol) {
      throw ConfigurationException("Protocol not found for generation: " +
                                   generation);
    }

    auto tower = std::make_shared<CellTower>(location, protocol, bandwidthMHz,
                                             num_cores);
    towers.add(tower);

    Logger::success("Created " + generation + " tower at " + location +
                    " with " + std::to_string(num_cores) + " cores");
  } catch (const CellularNetworkException &e) {
    Logger::error(std::string("Failed to create tower: ") + e.what());
    throw;
  }
}

std::shared_ptr<CellTower> Simulator::get_tower(int index) const {
  return towers.get(index);
}

std::shared_ptr<UserDevice> Simulator::connect_device_deferred_registration(
    const std::string &generation, const std::string &name,
    CommunicationType type, int towerIndex) {
  try {
    auto tower = get_tower(towerIndex);
    if (!tower) {
      throw ConfigurationException("Tower not found at index: " +
                                   std::to_string(towerIndex));
    }

    auto device = create_device(generation, name, type);

    // Place device near tower. rand() is not required to be reentrant, and
    // this path is called concurrently from ThreadPool workers (see
    // run_threading_benchmark and the parallel provisioning APIs below), so
    // a thread_local generator is used instead of the shared libc rand()
    // state - both for defined behavior and to avoid the workers
    // serializing on whatever internal lock a given libc's rand() uses.
    thread_local std::mt19937 positionRng(std::random_device{}());
    std::uniform_real_distribution<double> radiusDist(50.0, 350.0);
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * 3.141592653589793);
    double r = radiusDist(positionRng);
    double theta = angleDist(positionRng);
    device->set_position(tower->get_x() + r * cos(theta), tower->get_y() + r * sin(theta));

    bool connected = tower->connect_device(device);
    if (connected) {
      stats.total_connections++;
      Logger::success("Connected device " + name + " to tower at " +
                      tower->get_location());
    } else {
      stats.failed_connections++;
      Logger::warning("Failed to connect device " + name);
    }
    return device;
  } catch (const CellularNetworkException &e) {
    stats.failed_connections++;
    Logger::error(std::string("Connection failed: ") + e.what());
    throw;
  }
}

void Simulator::create_and_connect_device(const std::string &generation,
                                          const std::string &name,
                                          CommunicationType type,
                                          int towerIndex) {
  auto device = connect_device_deferred_registration(generation, name, type, towerIndex);
  devices.add(device);
}

void Simulator::connect_devices_async(const std::string &generation, int count,
                                      int towerIndex) {
  std::vector<std::future<void>> futures;

  Logger::info("Starting async connection of " + std::to_string(count) +
               " devices...");

  for (int i = 0; i < count; ++i) {
    futures.push_back(
        std::async(std::launch::async, [this, generation, i, towerIndex]() {
          try {
            // Simulate some work/delay
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            std::string name = "Async-Device-" + std::to_string(i);
            create_and_connect_device(generation, name, CommunicationType::DATA,
                                      towerIndex);
          } catch (...) {
            // Ignore errors in async tasks for this demo
          }
        }));
  }

  // Wait for all tasks to complete
  for (auto &f : futures) {
    f.wait();
  }

  Logger::success("Async connection of " + std::to_string(count) +
                  " devices completed");
}

void Simulator::generate_network_parallel(const std::string &generation, int count, int towerIndex) {
  std::vector<std::future<void>> futures;
  
  Logger::info("Starting parallel generation of " + std::to_string(count) + " devices...");
  
  for (int i = 0; i < count; ++i) {
    futures.push_back(threadPool.enqueue([this, generation, i, towerIndex] {
      try {
        std::string name = "Parallel-Device-" + std::to_string(i);
        create_and_connect_device(generation, name, CommunicationType::DATA, towerIndex);
      } catch (...) {
        // Ignore errors in parallel tasks
      }
    }));
  }
  
  // Wait for all tasks to complete
  for (auto &f : futures) {
    f.wait();
  }
  
  Logger::success("Parallel generation completed");
}

void Simulator::run_simulation(const std::string &generation) {
  OutputFormatter::print_header("Running " + generation +
                                " Network Simulation");

  auto protocol = get_protocol(generation);
  if (!protocol) {
    Logger::error("Protocol not found for " + generation);
    return;
  }

  IOHelpers::print(protocol->get_protocol_details()), IOHelpers::printNewline();
}

void Simulator::display_all_towers() const {
  OutputFormatter::print_header("CELL TOWERS SUMMARY");

  if (towers.size() == 0) {
    IOHelpers::print(" "), IOHelpers::print("No towers in the network."), IOHelpers::printNewline();
    return;
  }

  std::vector<std::string> headers = {"ID", "Location", "Gen", "Bandwidth", "Cores", "Devices", "Utilization"};
  std::vector<int> widths = {4, 25, 6, 12, 6, 10, 15};
  OutputFormatter::print_table_header(headers, widths);

  for (int i = 0; i < towers.size(); i++) {
    auto tower = towers.get(i);
    if (tower) {
      double util = tower->get_utilization();
      std::vector<std::string> row = {
        std::to_string(i),
        tower->get_location(),
        tower->get_protocol()->get_protocol_name(),
        std::to_string((int)tower->get_total_bandwidth()) + " MHz",
        std::to_string(tower->get_number_of_cores()),
        std::to_string(tower->get_current_device_count()) + "/" + std::to_string(tower->get_max_supported_devices()),
        std::to_string((int)util) + "%"
      };
      OutputFormatter::print_table_row(row, widths);
    }
  }
  OutputFormatter::print_table_separator(widths);
  IOHelpers::printNewline();
}

void Simulator::display_all_devices() const {
  OutputFormatter::print_header("USER DEVICES SUMMARY");

  if (devices.size() == 0) {
    IOHelpers::print(" "), IOHelpers::print("No devices in the network."), IOHelpers::printNewline();
    return;
  }

  std::vector<std::string> headers = {"ID", "Name", "Gen", "Comm Type", "Msgs", "Status", "Tower"};
  std::vector<int> widths = {5, 20, 5, 15, 6, 12, 8};
  OutputFormatter::print_table_header(headers, widths);

  for (int i = 0; i < devices.size(); i++) {
    auto device = devices.get(i);
    if (device) {
      std::string towerStr = "None";
      // Find connected tower index
      for (int t = 0; t < towers.size(); t++) {
        auto tower = towers.get(t);
        if (tower && tower->get_device(device->get_device_id()) != nullptr) {
          towerStr = "#" + std::to_string(t);
          break;
        }
      }

      std::vector<std::string> row = {
        std::to_string(device->get_device_id()),
        device->get_device_name(),
        device->get_device_type(),
        device->get_communication_type_string(),
        std::to_string(device->get_message_count()),
        device->get_connection_status() ? "Connected" : "Disconnected",
        towerStr
      };
      OutputFormatter::print_table_row(row, widths);
    }
  }
  OutputFormatter::print_table_separator(widths);
  IOHelpers::printNewline();
}

void Simulator::display_statistics() const {
  OutputFormatter::print_header("SIMULATION METRICS DASHBOARD");

  double totalUtil = 0.0;
  for (int i = 0; i < towers.size(); i++) {
    auto tower = towers.get(i);
    if (tower) {
      totalUtil += tower->get_utilization();
    }
  }
  double avgUtil = towers.size() > 0 ? (totalUtil / towers.size()) : 0.0;

  std::vector<std::pair<std::string, std::string>> statsData = {
      {"Total Cell Towers", std::to_string(towers.size())},
      {"Total Registered Devices", std::to_string(devices.size())},
      {"Successful Connections", std::to_string(stats.total_connections)},
      {"Failed Connections", std::to_string(stats.failed_connections)},
      {"Total Network Messages", std::to_string(stats.total_messages)},
      {"Average Network Utilization", std::to_string((int)avgUtil) + "%"}
  };

  OutputFormatter::print_summary_table(statsData, 60);
  IOHelpers::printNewline();
}

void Simulator::display_protocol_info(const std::string &generation) const {
  OutputFormatter::print_header(generation + " Protocol Information");

  auto protocol = get_protocol(generation);
  if (protocol) {
    IOHelpers::print(protocol->get_protocol_details()), IOHelpers::printNewline();
  } else {
    IOHelpers::print("Protocol not found for "), IOHelpers::print(generation), IOHelpers::printNewline();
  }
}

std::shared_ptr<Protocol>
Simulator::get_protocol(const std::string &generation) const {
  auto it = available_protocols.find(generation);
  if (it != available_protocols.end()) {
    return it->second;
  }
  return nullptr;
}

void Simulator::analyze2G() const {
  OutputFormatter::print_header("2G Network Analysis");

  auto protocol = get_protocol("2G");
  if (!protocol) {
    IOHelpers::print("2G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  
  
  
  
  
  

  // Basic Configuration
  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Basic Results
  IOHelpers::print(" Capacity Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels:"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  // QoS Metrics
  int simulated_users = max_users / 2; // Simulate 50% load
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("2G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  // Signal Quality Metrics
  auto signal = AdvancedMetrics::calculateSignalMetrics("2G", bandwidth, 1);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  // Network KPIs
  double utilization = 50.0; // Simulated utilization
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("2G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  // Traffic Patterns
  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  // Interference Analysis
  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("2G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  // Capacity Forecast
  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 5.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  // Additional Insights
  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("2G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("2G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-200 kHz)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::analyze3G() const {
  OutputFormatter::print_header("3G Network Analysis");

  auto protocol = get_protocol("3G");
  if (!protocol) {
    IOHelpers::print("3G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  
  
  
  
  
  

  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" Capacity Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels:"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  int simulated_users = max_users / 2;
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("3G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  auto signal = AdvancedMetrics::calculateSignalMetrics("3G", bandwidth, 1);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  double utilization = 50.0;
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("3G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("3G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 8.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("3G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("3G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-200 kHz)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::analyze4G() const {
  OutputFormatter::print_header("4G Network Analysis");

  auto protocol = get_protocol("4G");
  if (!protocol) {
    IOHelpers::print("4G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  auto ofdm = std::dynamic_pointer_cast<OFDMProtocol>(protocol);
  int numAntennas = ofdm ? ofdm->get_num_antennas() : 4;

  
  
  
  
  
  
  

  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("MIMO Antennas:"), IOHelpers::print(" "), IOHelpers::print(numAntennas), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" Capacity Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels:"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Note:"), IOHelpers::print(" Channels reused across "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  int simulated_users = max_users / 2;
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("4G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  auto signal =
      AdvancedMetrics::calculateSignalMetrics("4G", bandwidth, numAntennas);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  double utilization = 50.0;
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("4G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("4G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 12.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("4G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("4G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-10 kHz, Antenna 0)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::analyze5G() const {
  OutputFormatter::print_header("5G Network Analysis");

  auto protocol = get_protocol("5G");
  if (!protocol) {
    IOHelpers::print("5G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  auto mimo = std::dynamic_pointer_cast<MassiveMIMOProtocol>(protocol);
  int numAntennas = mimo ? mimo->get_num_antennas() : 16;
  double highFreqBand = mimo ? mimo->get_high_frequency_band() : 10.0;

  
  
  
  
  
  
  

  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Base Band Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("High Frequency Band:"), IOHelpers::print(" "), IOHelpers::print(highFreqBand), IOHelpers::print(" MHz at 1800 MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Base Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Massive MIMO Antennas:"), IOHelpers::print(" "), IOHelpers::print(numAntennas), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels (base):"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Note:"), IOHelpers::print(" Channels reused across "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  int simulated_users = max_users / 2;
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("5G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  auto signal =
      AdvancedMetrics::calculateSignalMetrics("5G", bandwidth, numAntennas);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  double utilization = 50.0;
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("5G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("5G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 20.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("5G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("5G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-10 kHz, Antenna 0)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::analyze6G() const {
  OutputFormatter::print_header("6G Network Analysis");

  auto protocol = get_protocol("6G");
  if (!protocol) {
    IOHelpers::print("6G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  auto terahertz = std::dynamic_pointer_cast<TerahertzProtocol>(protocol);
  int numAntennas = terahertz ? terahertz->get_num_antennas() : 64;
  double terahertzBand = terahertz ? terahertz->get_terahertz_band() : 100.0;

  
  
  
  
  
  
  

  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Base Band Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Terahertz Band:"), IOHelpers::print(" "), IOHelpers::print(terahertzBand), IOHelpers::print(" MHz at 300 GHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Base Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Ultra-Massive MIMO:"), IOHelpers::print(" "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" Capacity Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels (base):"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Note:"), IOHelpers::print(" Channels reused across "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas with AI optimization"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Features:"), IOHelpers::print(" "), IOHelpers::print("Quantum Encryption, AI Beamforming"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  int simulated_users = max_users / 2;
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("6G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  auto signal =
      AdvancedMetrics::calculateSignalMetrics("6G", bandwidth, numAntennas);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  double utilization = 50.0;
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("6G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("6G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 30.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("6G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("6G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-5 kHz, Antenna 0)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::analyze7G() const {
  OutputFormatter::print_header("7G Network Analysis");

  auto protocol = get_protocol("7G");
  if (!protocol) {
    IOHelpers::print("7G protocol not available"), IOHelpers::printNewline();
    return;
  }

  double bandwidth = 1.0;
  int max_users = protocol->calculate_max_users(bandwidth);
  int messages_per_user = protocol->calculate_messages_per_user();
  int required_cores =
      protocol->calculate_required_cores(max_users, messages_per_user);

  auto holographic = std::dynamic_pointer_cast<HolographicProtocol>(protocol);
  int numAntennas = holographic ? holographic->get_num_antennas() : 128;
  double satelliteBand =
      holographic ? holographic->get_satellite_band() : 500.0;

  
  
  
  
  
  
  

  IOHelpers::print(" Configuration"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Base Band Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(bandwidth), IOHelpers::print(" MHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Satellite Mesh Band:"), IOHelpers::print(" "), IOHelpers::print(satelliteBand), IOHelpers::print(" MHz (Visible Light)"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Channel Bandwidth:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_channel_bandwidth()), IOHelpers::print(" kHz"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Users per Base Channel:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Extreme MIMO:"), IOHelpers::print(" "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Holographic Multiplier:"), IOHelpers::print(" "), IOHelpers::print("2x"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" Capacity Results"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Maximum Supported Users:"), IOHelpers::print(" "), IOHelpers::print(max_users), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Total Channels (base):"), IOHelpers::print(" "), IOHelpers::print(protocol->calculate_total_channels(bandwidth)), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Messages per User:"), IOHelpers::print(" "), IOHelpers::print(messages_per_user), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Required Cores:"), IOHelpers::print(" "), IOHelpers::print(required_cores), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Note:"), IOHelpers::print(" Channels reused across "), IOHelpers::print(numAntennas), IOHelpers::print(" antennas with 2x multiplier"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Features:"), IOHelpers::print(" "), IOHelpers::print("Holographic Beamforming, Satellite Mesh, Brain Interface"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  // Advanced Metrics
  OutputFormatter::print_sub_header("ADVANCED PERFORMANCE METRICS", 80);
  IOHelpers::printNewline();

  int simulated_users = max_users / 2;
  auto qos =
      AdvancedMetrics::calculateQoSMetrics("7G", simulated_users, bandwidth);
  AdvancedMetrics::displayQoSMetrics(qos);
  IOHelpers::printNewline();

  auto signal =
      AdvancedMetrics::calculateSignalMetrics("7G", bandwidth, numAntennas);
  AdvancedMetrics::displaySignalMetrics(signal);
  IOHelpers::printNewline();

  double utilization = 50.0;
  auto kpis = AdvancedMetrics::calculateNetworkKPIs("7G", utilization);
  AdvancedMetrics::displayNetworkKPIs(kpis);
  IOHelpers::printNewline();

  auto traffic = AdvancedMetrics::generateTrafficPatterns(max_users);
  AdvancedMetrics::displayTrafficPatterns(traffic);

  int num_channels = protocol->calculate_total_channels(bandwidth);
  auto interference =
      AdvancedMetrics::analyzeInterference("7G", num_channels, 1);
  AdvancedMetrics::displayInterferenceAnalysis(interference);
  IOHelpers::printNewline();

  auto forecast =
      AdvancedMetrics::forecastCapacity(simulated_users, max_users, 50.0);
  AdvancedMetrics::displayCapacityForecast(forecast);
  IOHelpers::printNewline();

  OutputFormatter::print_sub_header("NETWORK INSIGHTS", 80);
  double spectral_eff = AdvancedMetrics::calculateSpectralEfficiency("7G");
  double energy_eff =
      AdvancedMetrics::calculateEnergyEfficiency("7G", simulated_users);

  IOHelpers::print(" "), IOHelpers::print("Spectral Efficiency:"), IOHelpers::print(" "), IOHelpers::print(spectral_eff), IOHelpers::print(" bits/Hz/cell"), IOHelpers::printNewline();
  IOHelpers::print(" "), IOHelpers::print("Energy Efficiency:"), IOHelpers::print(" "), IOHelpers::print(energy_eff), IOHelpers::print(" bits/Joule"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  OutputFormatter::print_section("First Channel (0-3 kHz, Antenna 0)");
  IOHelpers::print(" "), IOHelpers::print("Capacity:"), IOHelpers::print(" "), IOHelpers::print(protocol->get_users_per_channel()), IOHelpers::print(" users"), IOHelpers::printNewline();
}

void Simulator::run_threading_benchmark(int devices, int threads, const std::string &csvPath) {
  if (threads <= 0) {
    threads = std::max(1u, std::thread::hardware_concurrency());
  }

  // This benchmark compares connecting `devices` devices to `threads`
  // independent 5G towers, sequentially vs. through the ThreadPool, one
  // worker per tower's share of devices. Two things this deliberately does
  // NOT do, because they would misrepresent what's being measured:
  //   1. No artificial sleep/busy-loop per device. An earlier version added
  //      a synthetic delay to "make parallelism worth it," which meant the
  //      reported speedup mostly reflected N threads sleeping concurrently,
  //      not the cost of the real connection path (capacity checks, channel
  //      allocation, core registration, collection inserts, logging).
  //   2. No single shared mutex wrapping the whole connect call. That lock
  //      fully serialized the multi-threaded run's real work, so only the
  //      (fake) work outside the lock actually ran in parallel. CellTower
  //      and NetworkCollection<T> now guard their own state internally, and
  //      each thread here is given its own tower, so lock contention is
  //      real but not total.
  // Devices are spread round-robin across `threads` towers so the single-
  // and multi-threaded runs do byte-for-byte the same work, just scheduled
  // differently - that's what makes the comparison fair.
  std::atomic<int> connected_single{0};
  std::atomic<int> failures_single{0};
  std::atomic<int> connected_multi{0};
  std::atomic<int> failures_multi{0};

  // Per-device success/failure logging would otherwise dominate the timed
  // section (string formatting + vector growth + queue push per device),
  // so it's off for the duration of the benchmark and restored afterward.
  Logger::set_quiet(true);

  // Each CellularCore for the 5G protocol holds ~92 devices (fixed by
  // message overhead, independent of bandwidth), and max_supported_devices
  // is min(channel capacity, core capacity) - so undersized towers hit a
  // capacity wall partway through the run. Past that wall, every further
  // connection attempt throws and is caught, and exception handling is
  // expensive enough to dominate the timing - at that point the benchmark
  // is measuring exception overhead, not connection throughput. Sizing
  // cores to the actual per-tower share of devices (with a safety margin)
  // keeps every device connectable so the timed section reflects the real
  // connection path end to end.
  const int devicesPerTower = threads > 0 ? (devices + threads - 1) / threads : devices;
  const int coresPerTower = std::max(10, static_cast<int>(std::ceil(devicesPerTower / 92.0 * 1.5)));

  auto make_towers = [this, coresPerTower](const std::string &prefix, int count) {
    std::vector<int> indices;
    for (int t = 0; t < count; ++t) {
      try {
        create_tower("5G", prefix + "-" + std::to_string(t), 20.0, coresPerTower);
        indices.push_back(get_tower_count() - 1);
      } catch (...) {
        // Tower creation failure just means that slot gets skipped below.
      }
    }
    return indices;
  };

  // === RUN 1: Single-threaded baseline ===
  std::vector<int> singleTowers = make_towers("Benchmark-Single", threads);
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < devices; ++i) {
    if (singleTowers.empty()) break;
    try {
      std::string name = "Bench-Dev-" + std::to_string(i);
      int towerIdx = singleTowers[i % singleTowers.size()];
      create_and_connect_device("5G", name, CommunicationType::DATA, towerIdx);
      connected_single++;
    } catch (...) {
      failures_single++;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  double singleMs = std::chrono::duration<double, std::milli>(end - start).count();

  // === RUN 2: Same workload, batched one chunk per tower/worker ===
  // A first version of this benchmark enqueued one ThreadPool task per
  // device (4000 devices -> 4000 tasks). Connecting one device is on the
  // order of a few microseconds of real work, which is smaller than the
  // ThreadPool's own per-task dispatch cost (queue mutex, condition
  // variable wake, packaged_task/future allocation) - so that version
  // measured scheduling overhead, not the benefit of parallelism, and
  // actually came out slower than the sequential run. Batching devices
  // into one task per tower amortizes that dispatch cost across hundreds
  // of connections instead of one, which is what makes the parallel work
  // actually dominate the measurement instead of the scheduler.
  std::vector<int> multiTowers = make_towers("Benchmark-Multi", threads);
  {
    ThreadPool benchPool(threads);

    start = std::chrono::high_resolution_clock::now();
    std::vector<std::future<void>> futures;
    for (size_t slot = 0; slot < multiTowers.size(); ++slot) {
      int towerIdx = multiTowers[slot];
      futures.push_back(benchPool.enqueue([this, slot, devices, threads, towerIdx, &connected_multi, &failures_multi] {
        // Each worker publishes into the shared `devices` collection once,
        // as a batch, instead of once per device. With one lock per device
        // (the original approach), every worker's per-device work is tiny
        // enough that time spent contending for the devices-collection
        // mutex dominates the run and erases the benefit of using separate
        // towers per thread in the first place - the earlier version of
        // this benchmark plateaued at ~1.0x speedup for exactly that
        // reason. Batching the publish step is the standard fix: keep the
        // lock, just take it far less often.
        std::vector<std::shared_ptr<UserDevice>> localBatch;
        for (int i = static_cast<int>(slot); i < devices; i += threads) {
          try {
            std::string name = "Bench-Dev-PT-" + std::to_string(i);
            auto device = connect_device_deferred_registration("5G", name, CommunicationType::DATA, towerIdx);
            localBatch.push_back(device);
            connected_multi++;
          } catch (...) {
            failures_multi++;
          }
        }
        this->devices.add_batch(localBatch);
      }));
    }
    for (auto &f : futures) f.wait();
    end = std::chrono::high_resolution_clock::now();
  } // ThreadPool destroyed here after all futures are done

  double multiMs = std::chrono::duration<double, std::milli>(end - start).count();

  double speedup = singleMs / (multiMs > 0.0 ? multiMs : 1.0);

  // Enhanced output formatting
  OutputFormatter::print_header("THREADING BENCHMARK RESULTS");
  IOHelpers::printNewline();
  IOHelpers::print("  Devices:      "); IOHelpers::print(devices); IOHelpers::printNewline();
  IOHelpers::print("  Threads:      "); IOHelpers::print(threads); IOHelpers::printNewline();
  IOHelpers::printNewline();
  
  IOHelpers::print("  SINGLE-THREADED RESULTS:"); IOHelpers::printNewline();
  IOHelpers::print("    Time:       "); IOHelpers::printDouble(singleMs, 2); IOHelpers::print(" ms"); IOHelpers::printNewline();
  IOHelpers::print("    Connected:  "); IOHelpers::print((int)connected_single); IOHelpers::printNewline();
  IOHelpers::print("    Failures:   "); IOHelpers::print((int)failures_single); IOHelpers::printNewline();
  IOHelpers::printNewline();
  
  IOHelpers::print("  MULTI-THREADED RESULTS ("); IOHelpers::print(threads); IOHelpers::print(" threads):"); IOHelpers::printNewline();
  IOHelpers::print("    Time:       "); IOHelpers::printDouble(multiMs, 2); IOHelpers::print(" ms"); IOHelpers::printNewline();
  IOHelpers::print("    Connected:  "); IOHelpers::print((int)connected_multi); IOHelpers::printNewline();
  IOHelpers::print("    Failures:   "); IOHelpers::print((int)failures_multi); IOHelpers::printNewline();
  IOHelpers::printNewline();
  
  IOHelpers::print("  PERFORMANCE:"); IOHelpers::printNewline();
  IOHelpers::print("    Speedup:    "); IOHelpers::printDouble(speedup, 2); IOHelpers::print("x"); IOHelpers::printNewline();
  
  // Realistic status based on actual multi-threading performance
  if (threads == 1) {
    // For single thread, expect ~1.0x (overhead should be minimal)
    if (speedup >= 0.95) {
      IOHelpers::print("    Status:     Excellent (minimal overhead)"); IOHelpers::printNewline();
    } else if (speedup >= 0.85) {
      IOHelpers::print("    Status:     Good (acceptable overhead)"); IOHelpers::printNewline();
    } else if (speedup >= 0.70) {
      IOHelpers::print("    Status:     Moderate (noticeable overhead)"); IOHelpers::printNewline();
    } else {
      IOHelpers::print("    Status:     Poor (high ThreadPool overhead)"); IOHelpers::printNewline();
    }
  } else {
    // For multiple threads, use realistic thresholds
    if (speedup >= 5.0) {
      IOHelpers::print("    Status:     Excellent parallelization ("); 
      IOHelpers::printDouble(speedup, 2);
      IOHelpers::print("x speedup)");
      IOHelpers::printNewline();
    } else if (speedup >= 2.0) {
      IOHelpers::print("    Status:     Good parallelization (");
      IOHelpers::printDouble(speedup, 2);
      IOHelpers::print("x speedup)");
      IOHelpers::printNewline();
    } else if (speedup >= 1.5) {
      IOHelpers::print("    Status:     Moderate parallelization (");
      IOHelpers::printDouble(speedup, 2);
      IOHelpers::print("x speedup)");
      IOHelpers::printNewline();
    } else if (speedup >= 1.0) {
      IOHelpers::print("    Status:     Low performance (");
      IOHelpers::printDouble(speedup, 2);
      IOHelpers::print("x speedup, high overhead)");
      IOHelpers::printNewline();
    } else {
      IOHelpers::print("    Status:     Very poor (slower than single-threaded!)"); IOHelpers::printNewline();
    }
  }
  IOHelpers::printNewline();

  if (!csvPath.empty()) {
    // write simple CSV
    std::ofstream out(csvPath);
    if (out) {
      out << "devices,threads,single_ms,multi_ms,speedup,connected_single,failures_single,connected_multi,failures_multi\n";
      out << devices << "," << threads << "," << singleMs << "," << multiMs << "," << speedup << ","
          << (int)connected_single << "," << (int)failures_single << ","
          << (int)connected_multi << "," << (int)failures_multi << "\n";
      out.close();
      IOHelpers::print("  Results saved to: "); IOHelpers::print(csvPath.c_str()); IOHelpers::printNewline();
    } else {
      IOHelpers::print("  Failed to write CSV to: "); IOHelpers::print(csvPath.c_str()); IOHelpers::printNewline();
    }
  }

  Logger::set_quiet(false);
}

void Simulator::apply_beamforming_to_tower(int towerIndex, double factor) {
  auto tower = get_tower(towerIndex);
  if (!tower) throw ConfigurationException("Tower index invalid");
  tower->apply_beamforming(factor);
}

void Simulator::disable_beamforming_on_tower(int towerIndex) {
  auto tower = get_tower(towerIndex);
  if (!tower) throw ConfigurationException("Tower index invalid");
  tower->disable_beamforming();
}

// Device Analytics Implementation
void Simulator::display_device_analytics() const {
  OutputFormatter::print_header("COMPREHENSIVE DEVICE ANALYTICS");
  IOHelpers::printNewline();
  
  display_device_breakdown_by_generation();
  IOHelpers::printNewline();
  
  display_device_breakdown_by_tower();
  IOHelpers::printNewline();
  
  display_device_breakdown_by_frequency();
  IOHelpers::printNewline();
  
  display_device_breakdown_by_area();
}

void Simulator::display_device_breakdown_by_generation() const {
  OutputFormatter::print_sub_header("DEVICE BREAKDOWN BY GENERATION");
  IOHelpers::printNewline();
  
  std::map<std::string, int> genCount;
  std::map<std::string, int> genData;
  std::map<std::string, int> genVoice;
  std::map<std::string, int> genBoth;
  
  // Count devices by generation
  auto allDevices = devices.get_all();
  for (const auto& dev : allDevices) {
    if (!dev) continue;
    std::string gen = dev->get_device_type(); // e.g., "2G", "3G", etc.
    genCount[gen]++;
    
    auto commType = dev->get_communication_type();
    if (commType == CommunicationType::DATA) genData[gen]++;
    else if (commType == CommunicationType::VOICE) genVoice[gen]++;
    else if (commType == CommunicationType::BOTH) genBoth[gen]++;
  }
  
  // Display table
  std::vector<std::string> headers = {"Generation", "Total", "Data", "Voice", "Both", "Percentage"};
  std::vector<int> widths = {12, 8, 8, 8, 8, 12};
  OutputFormatter::print_table_header(headers, widths);
  
  int totalDevices = allDevices.size();
  for (const auto& pair : genCount) {
    double percentage = (totalDevices > 0) ? (pair.second * 100.0 / totalDevices) : 0.0;
    std::vector<std::string> row = {
      pair.first,
      std::to_string(pair.second),
      std::to_string(genData[pair.first]),
      std::to_string(genVoice[pair.first]),
      std::to_string(genBoth[pair.first]),
      std::to_string((int)percentage) + "%"
    };
    OutputFormatter::print_table_row(row, widths);
  }
  OutputFormatter::print_table_separator(widths);
  
  IOHelpers::print("Total Devices: "); 
  IOHelpers::print(totalDevices);
  IOHelpers::printNewline();
}

void Simulator::display_device_breakdown_by_tower() const {
  OutputFormatter::print_sub_header("DEVICE BREAKDOWN BY TOWER");
  IOHelpers::printNewline();
  
  std::vector<std::string> headers = {"Tower", "Location", "Protocol", "Devices", "Capacity", "Utilization"};
  std::vector<int> widths = {8, 20, 12, 10, 10, 12};
  OutputFormatter::print_table_header(headers, widths);
  
  auto allTowers = towers.get_all();
  for (size_t i = 0; i < allTowers.size(); ++i) {
    auto tower = allTowers[i];
    if (!tower) continue;
    
    int deviceCount = tower->get_current_device_count();
    int capacity = tower->get_max_supported_devices();
    double util = tower->get_utilization();
    
    // Get protocol name from tower's protocol
    std::string protocolName = "Unknown";
    auto proto = tower->get_protocol();
    if (proto) {
      protocolName = proto->get_protocol_name();
    }
    
    std::vector<std::string> row = {
      std::to_string(i),
      tower->get_location(),
      protocolName,
      std::to_string(deviceCount),
      std::to_string(capacity),
      std::to_string((int)util) + "%"
    };
    OutputFormatter::print_table_row(row, widths);
  }
  OutputFormatter::print_table_separator(widths);
}

void Simulator::display_device_breakdown_by_frequency() const {
  OutputFormatter::print_sub_header("DEVICE BREAKDOWN BY FREQUENCY BAND");
  IOHelpers::printNewline();
  
  std::map<std::string, int> freqBands;
  std::map<std::string, double> totalBandwidth;
  
  auto allTowers = towers.get_all();
  for (const auto& tower : allTowers) {
    if (!tower) continue;
    
    double bw = tower->get_total_bandwidth();
    std::string band;
    
    // Classify by frequency band
    if (bw < 5.0) band = "Low (<5 MHz)";
    else if (bw < 15.0) band = "Medium (5-15 MHz)";
    else if (bw < 30.0) band = "High (15-30 MHz)";
    else band = "Very High (>30 MHz)";
    
    freqBands[band] += tower->get_current_device_count();
    totalBandwidth[band] += bw;
  }
  
  std::vector<std::string> headers = {"Frequency Band", "Devices", "Total BW (MHz)", "Avg BW"};
  std::vector<int> widths = {20, 10, 16, 12};
  OutputFormatter::print_table_header(headers, widths);
  
  for (const auto& pair : freqBands) {
    int towerCount = 0;
    for (const auto& tower : allTowers) {
      if (!tower) continue;
      double bw = tower->get_total_bandwidth();
      std::string band;
      if (bw < 5.0) band = "Low (<5 MHz)";
      else if (bw < 15.0) band = "Medium (5-15 MHz)";
      else if (bw < 30.0) band = "High (15-30 MHz)";
      else band = "Very High (>30 MHz)";
      if (band == pair.first) towerCount++;
    }
    
    double avgBw = (towerCount > 0) ? (totalBandwidth[pair.first] / towerCount) : 0.0;
    
    std::vector<std::string> row = {
      pair.first,
      std::to_string(pair.second),
      std::to_string((int)totalBandwidth[pair.first]),
      std::to_string((int)avgBw) + " MHz"
    };
    OutputFormatter::print_table_row(row, widths);
  }
  OutputFormatter::print_table_separator(widths);
}

void Simulator::display_device_breakdown_by_area() const {
  OutputFormatter::print_sub_header("DEVICE BREAKDOWN BY COVERAGE AREA");
  IOHelpers::printNewline();
  
  std::map<std::string, int> areaDevices;
  std::map<std::string, int> areaTowers;
  
  auto allTowers = towers.get_all();
  for (const auto& tower : allTowers) {
    if (!tower) continue;
    
    std::string location = tower->get_location();
    int deviceCount = tower->get_current_device_count();
    
    areaDevices[location] += deviceCount;
    areaTowers[location]++;
  }
  
  std::vector<std::string> headers = {"Area/Location", "Towers", "Devices", "Avg/Tower"};
  std::vector<int> widths = {25, 10, 10, 12};
  OutputFormatter::print_table_header(headers, widths);
  
  for (const auto& pair : areaDevices) {
    int towerCount = areaTowers[pair.first];
    double avgPerTower = (towerCount > 0) ? ((double)pair.second / towerCount) : 0.0;
    
    std::vector<std::string> row = {
      pair.first,
      std::to_string(towerCount),
      std::to_string(pair.second),
      std::to_string((int)avgPerTower)
    };
    OutputFormatter::print_table_row(row, widths);
  }
  OutputFormatter::print_table_separator(widths);
  
  IOHelpers::printNewline();
  IOHelpers::print("Summary:");
  IOHelpers::printNewline();
  IOHelpers::print("  Total Coverage Areas: ");
  IOHelpers::print((int)areaDevices.size());
  IOHelpers::printNewline();
  IOHelpers::print("  Total Towers: ");
  IOHelpers::print((int)allTowers.size());
  IOHelpers::printNewline();
}

void Simulator::display_total_devices_summary() const {
  OutputFormatter::print_sub_header("TOTAL DEVICES SUMMARY");
  IOHelpers::printNewline();
  
  auto allDevices = devices.get_all();
  int totalDevices = allDevices.size();
  
  if (totalDevices == 0) {
    IOHelpers::print("  No devices in the network.");
    IOHelpers::printNewline();
    return;
  }
  
  // Count by communication type
  int dataOnly = 0, voiceOnly = 0, both = 0;
  for (const auto& dev : allDevices) {
    if (!dev) continue;
    auto commType = dev->get_communication_type();
    if (commType == CommunicationType::DATA) dataOnly++;
    else if (commType == CommunicationType::VOICE) voiceOnly++;
    else if (commType == CommunicationType::BOTH) both++;
  }
  
  // Count by generation
  std::map<std::string, int> genCount;
  for (const auto& dev : allDevices) {
    if (!dev) continue;
    genCount[dev->get_device_type()]++;
  }
  
  // Display overview
  IOHelpers::print("Total Devices: ");
  IOHelpers::print(totalDevices);
  IOHelpers::printNewline();
  IOHelpers::print("Total Towers: ");
  IOHelpers::print((int)towers.size());
  IOHelpers::printNewline();
  IOHelpers::printNewline();
  
  // Communication type breakdown
  IOHelpers::print("Communication Type Breakdown:");
  IOHelpers::printNewline();
  IOHelpers::print("  Data Only:  ");
  IOHelpers::print(dataOnly);
  IOHelpers::print(" (");
  IOHelpers::print((int)(dataOnly * 100.0 / totalDevices));
  IOHelpers::print("%)");
  IOHelpers::printNewline();
  
  IOHelpers::print("  Voice Only: ");
  IOHelpers::print(voiceOnly);
  IOHelpers::print(" (");
  IOHelpers::print((int)(voiceOnly * 100.0 / totalDevices));
  IOHelpers::print("%)");
  IOHelpers::printNewline();
  
  IOHelpers::print("  Data+Voice: ");
  IOHelpers::print(both);
  IOHelpers::print(" (");
  IOHelpers::print((int)(both * 100.0 / totalDevices));
  IOHelpers::print("%)");
  IOHelpers::printNewline();
  IOHelpers::printNewline();
  
  // Generation breakdown
  IOHelpers::print("Generation Breakdown:");
  IOHelpers::printNewline();
  for (const auto& pair : genCount) {
    IOHelpers::print("  ");
    IOHelpers::print(pair.first.c_str());
    IOHelpers::print(": ");
    IOHelpers::print(pair.second);
    IOHelpers::print(" (");
    IOHelpers::print((int)(pair.second * 100.0 / totalDevices));
    IOHelpers::print("%)");
    IOHelpers::printNewline();
  }
}

void Simulator::reset() {
  towers.clear();
  devices.clear();
  stats.total_connections = 0;
  stats.failed_connections = 0;
  stats.total_messages = 0;
  stats.average_utilization = 0.0;
  
  // Clear and reset RF & routing states
  obstacles.assign(100, std::vector<bool>(100, false));
  // Pre-place some default buildings (obstacles) in 100x100 space
  for (int r = 30; r <= 50; r++) {
    for (int c = 25; c <= 32; c++) {
      obstacles[r][c] = true;
    }
  }
  for (int r = 60; r <= 65; r++) {
    for (int c = 60; c <= 80; c++) {
      obstacles[r][c] = true;
    }
  }
  active_packets.clear();
  next_packet_id = 1;
  dropped_packets = 0;
  processed_packets = 0;
  total_packets_routed = 0;
  handover_logs.clear();

  Logger::clear_logs();
  Logger::info("Simulator reset");
}

void Simulator::simulate_traffic_fluctuation() {
  // Randomly disconnect or connect devices to simulate traffic
  int action = rand() % 3;

  if (action == 0 && devices.size() > 0) {
    // Disconnect a random device
    int idx = rand() % devices.size();
    auto device = devices.get(idx);
    if (device && device->get_connection_status()) {
      // Find which tower it's connected to
      for (int i = 0; i < towers.size(); i++) {
        auto tower = towers.get(i);
        if (tower->disconnect_device(device->get_device_id())) {
          break;
        }
      }
    }
  } else if (action == 1 && towers.size() > 0) {
    // Connect a new temporary device
    int towerIdx = rand() % towers.size();
    std::string gen = "4G"; // Default
    auto tower = towers.get(towerIdx);
    if (tower) {
      std::string proto = tower->get_protocol()->get_protocol_name();
      if (proto.find("2G") != std::string::npos)
        gen = "2G";
      else if (proto.find("3G") != std::string::npos)
        gen = "3G";
      else if (proto.find("5G") != std::string::npos)
        gen = "5G";
      else if (proto.find("6G") != std::string::npos)
        gen = "6G";
      else if (proto.find("7G") != std::string::npos)
        gen = "7G";

      try {
        create_and_connect_device(
            gen, "Temp-Device-" + std::to_string(rand() % 1000),
            CommunicationType::DATA, towerIdx);
      } catch (...) {
        // Ignore full tower errors during fluctuation
      }
    }
  }

  // Tick packet routing and generation
  trigger_packet_generation();
  update_packet_routing();
}

bool Simulator::check_line_of_sight(double x1, double y1, double x2, double y2) const {
  int gx1 = std::max(0, std::min(99, (int)(x1 / 10.0)));
  int gy1 = std::max(0, std::min(99, (int)(y1 / 10.0)));
  int gx2 = std::max(0, std::min(99, (int)(x2 / 10.0)));
  int gy2 = std::max(0, std::min(99, (int)(y2 / 10.0)));

  int dx = abs(gx2 - gx1);
  int dy = abs(gy2 - gy1);
  int sx = gx1 < gx2 ? 1 : -1;
  int sy = gy1 < gy2 ? 1 : -1;
  int err = dx - dy;

  int cx = gx1;
  int cy = gy1;

  while (true) {
    if (obstacles[cy][cx]) {
      if ((cx != gx1 || cy != gy1) && (cx != gx2 || cy != gy2)) {
        return false;
      }
    }
    if (cx == gx2 && cy == gy2) break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      cx += sx;
    }
    if (e2 < dx) {
      err += dx;
      cy += sy;
    }
  }
  return true;
}

double Simulator::compute_rssi(double distance, bool has_obstacle) const {
  double dist = std::max(1.0, distance);
  double exponent = 3.0; // Path loss exponent (urban)
  double rssi = -30.0 - 10.0 * exponent * log10(dist);
  if (has_obstacle) {
    rssi -= 25.0; // Concrete wall penetration loss (dB)
  }
  if (rssi > -30.0) rssi = -30.0;
  if (rssi < -120.0) rssi = -120.0;
  return rssi;
}

bool Simulator::has_obstacle_at(int gx, int gy) const {
  if (gx >= 0 && gx < 100 && gy >= 0 && gy < 100) {
    return obstacles[gy][gx];
  }
  return false;
}

void Simulator::toggle_obstacle_at(int gx, int gy) {
  if (gx >= 0 && gx < 100 && gy >= 0 && gy < 100) {
    bool state = !obstacles[gy][gx];
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int ny = gy + dy;
        int nx = gx + dx;
        if (nx >= 0 && nx < 100 && ny >= 0 && ny < 100) {
          obstacles[ny][nx] = state;
        }
      }
    }
  }
}

void Simulator::trigger_packet_generation() {
  int dev_count = devices.size();
  if (dev_count < 2) return;

  int num_pkts = rand() % 3 + 1;
  for (int p = 0; p < num_pkts; p++) {
    int src_idx = rand() % dev_count;
    auto src_dev = devices.get(src_idx);
    if (!src_dev || !src_dev->get_connection_status()) continue;

    int dest_idx = rand() % dev_count;
    if (dest_idx == src_idx) dest_idx = (dest_idx + 1) % dev_count;
    auto dst_dev = devices.get(dest_idx);
    if (!dst_dev || !dst_dev->get_connection_status()) continue;

    int src_tower = -1;
    for (int t = 0; t < towers.size(); t++) {
      auto tw = towers.get(t);
      if (tw && tw->get_device(src_dev->get_device_id())) {
        src_tower = t;
        break;
      }
    }

    int dst_tower = -1;
    for (int t = 0; t < towers.size(); t++) {
      auto tw = towers.get(t);
      if (tw && tw->get_device(dst_dev->get_device_id())) {
        dst_tower = t;
        break;
      }
    }

    if (src_tower == -1 || dst_tower == -1) continue;

    Packet pkt;
    pkt.id = next_packet_id++;
    pkt.source_id = src_idx;
    pkt.dest_id = dest_idx;
    pkt.progress = 0.0;
    pkt.current_hop = 0;
    pkt.src_tower_idx = src_tower;
    pkt.dst_tower_idx = dst_tower;
    pkt.type = (rand() % 2 == 0) ? "VOICE" : "DATA";

    active_packets.push_back(pkt);
    total_packets_routed++;
  }
}

void Simulator::update_packet_routing() {
  int router_max_capacity = 30;
  int packets_at_router = 0;
  for (const auto &pkt : active_packets) {
    if (pkt.current_hop == 1 || pkt.current_hop == 2) {
      packets_at_router++;
    }
  }

  std::vector<Packet> next_packets;
  for (auto &pkt : active_packets) {
    auto src_dev = devices.get(pkt.source_id);
    auto dst_dev = devices.get(pkt.dest_id);
    if (!src_dev || !dst_dev || !src_dev->get_connection_status() || !dst_dev->get_connection_status()) {
      dropped_packets++;
      continue;
    }

    pkt.progress += 0.25; // 4 steps per hop

    if (pkt.progress >= 1.0) {
      pkt.progress = 0.0;
      pkt.current_hop++;

      if (pkt.current_hop == 1) {
        if (packets_at_router >= router_max_capacity) {
          dropped_packets++;
          continue;
        }
      } else if (pkt.current_hop == 4) {
        processed_packets++;
        stats.total_messages++;
        continue;
      }
    }
    next_packets.push_back(pkt);
  }
  active_packets = next_packets;
}

void Simulator::optimize_network() {
  Logger::info("Starting network optimization...");

  int optimized_count = 0;
  for (int i = 0; i < towers.size(); i++) {
    auto tower = towers.get(i);
    if (tower->get_utilization() > 80.0) {
      // High utilization - in a real sim we might add carriers or offload
      // Here we'll just log a recommendation
      Logger::warning("Tower " + std::to_string(tower->get_tower_id()) +
                      " at " + tower->get_location() + " is overloaded (" +
                      std::to_string(tower->get_utilization()) + "%)");
      Logger::info(
          "Recommendation: Deploy small cell or add carrier frequency");
      optimized_count++;
    }
  }

  if (optimized_count == 0) {
    Logger::success("Network is operating within optimal parameters.");
  } else {
    Logger::info("Optimization analysis completed. " +
                 std::to_string(optimized_count) + " issues found.");
  }
}
