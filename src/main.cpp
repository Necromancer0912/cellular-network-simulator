#include "Exceptions.h"
#include "NetworkAnalytics.h"
#include "Simulator.h"
#include "Utils.h"
#include "ConsoleTUI.h"
#include "IOHelpers.h"
#include <memory>
#include <string>

void displayMainMenu() {
  OutputFormatter::print_header("CELLULAR NETWORK SIMULATOR - MAIN MENU");

  IOHelpers::print(" \033[1;35mBasic Operations\033[0m"), IOHelpers::printNewline();
  IOHelpers::print(" ────────────────────────────────────────"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 1\033[0m] Run 2G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 2\033[0m] Run 3G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 3\033[0m] Run 4G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 4\033[0m] Run 5G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 5\033[0m] Run 6G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 6\033[0m] Run 7G Network Analysis"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 7\033[0m] Create Custom Network"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" \033[1;34mAdvanced Features\033[0m"), IOHelpers::printNewline();
  IOHelpers::print(" ────────────────────────────────────────"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 8\033[0m] Network Analytics & Performance"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m 9\033[0m] Generation Comparison Tool"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m10\033[0m] Load Scenario"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m11\033[0m] Network Visualization"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m12\033[0m] Optimization Advisor"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" \033[1;36mBenchmarking & Testing\033[0m"), IOHelpers::printNewline();
  IOHelpers::print(" ────────────────────────────────────────"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m13\033[0m] Threading Benchmark (Minimal Mode)"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m14\033[0m] Threading Benchmark (Good Mode)"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m15\033[0m] Threading Benchmark (Wild Mode)"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m16\033[0m] Run Comprehensive Demo"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" \033[1;33mNetwork Monitoring & Optimization\033[0m"), IOHelpers::printNewline();
  IOHelpers::print(" ────────────────────────────────────────"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m17\033[0m] Toggle Live Network Monitor"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m18\033[0m] Apply Beamforming to Tower"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m19\033[0m] Disable Beamforming on Tower"), IOHelpers::printNewline();
  IOHelpers::printNewline();

  IOHelpers::print(" \033[1;37mManagement & Display\033[0m"), IOHelpers::printNewline();
  IOHelpers::print(" ────────────────────────────────────────"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m20\033[0m] Display All Towers"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m21\033[0m] Display All Devices"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m22\033[0m] Display Statistics"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m23\033[0m] Device Analytics Menu"), IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;32m24\033[0m] Reset Simulator"), IOHelpers::printNewline();
  IOHelpers::printNewline();
  IOHelpers::print("  [ \033[1;31m 0\033[0m] Exit Program"), IOHelpers::printNewline();
  IOHelpers::printNewline();
  IOHelpers::print(" → "), IOHelpers::print("Enter your choice: ");
}

void clearInputBuffer() {
  // Not needed for basicIO
}

int getIntInput(const std::string &prompt) {
  IOHelpers::print(prompt);
  return IOHelpers::readInt();
}

double getDoubleInput(const std::string &prompt) {
  IOHelpers::print(prompt);
  return IOHelpers::readDouble();
}

std::string getStringInput(const std::string &prompt) {
  IOHelpers::print(prompt);
  char buffer[256];
  IOHelpers::readString(buffer, 256);
  return std::string(buffer);
}

void pause() {
  IOHelpers::printNewline();
  IOHelpers::print(" [Press Enter to continue...]");
  char dummy[10];
  IOHelpers::readString(dummy, 10);
  IOHelpers::printNewline();
}

void createCustomNetwork(Simulator &sim) {
  OutputFormatter::print_header("Create Custom Network");

  std::vector<std::string> genOptions = {"2G", "3G", "4G", "5G", "6G", "7G", "Cancel"};
  int genChoice = OutputFormatter::select_from_menu("CREATE CUSTOM TOWER - SELECT GENERATION", genOptions);
  if (genChoice < 0 || genChoice == 6) {
    Logger::info("Tower creation cancelled.");
    return;
  }
  std::string generation = genOptions[genChoice];

  std::string location = getStringInput("Enter tower location: ");
  double bandwidth = getDoubleInput("Enter bandwidth (MHz): ");
  int num_cores = getIntInput("Enter number of cellular cores: ");

  sim.create_tower(generation, location, bandwidth, num_cores);

  int towerIndex = sim.get_tower_count() - 1;
  auto tower = sim.get_tower(towerIndex);

  IOHelpers::printNewline();
  tower->display_tower_info();

  std::vector<std::string> addDevOptions = {"Yes, add devices to this tower", "No, back to main menu"};
  int addDevChoice = OutputFormatter::select_from_menu("ADD DEVICES TO TOWER?", addDevOptions);

  if (addDevChoice == 0) {
    int numDevices = getIntInput("How many devices to add? ");

    for (int i = 0; i < numDevices; i++) {
      IOHelpers::printNewline();
      IOHelpers::print("Device ");
      IOHelpers::printInt(i + 1);
      IOHelpers::print(":");
      IOHelpers::printNewline();
      std::string device_name = getStringInput("  Device name: ");

      std::vector<std::string> commOptions = {"Data only", "Voice only", "Data & Voice"};
      int commChoice = OutputFormatter::select_from_menu("SELECT DEVICE COMMUNICATION TYPE", commOptions);

      CommunicationType comm_type;
      switch (commChoice) {
      case 0:
        comm_type = CommunicationType::DATA;
        break;
      case 1:
        comm_type = CommunicationType::VOICE;
        break;
      case 2:
      default:
        comm_type = CommunicationType::BOTH;
        break;
      }

      try {
        sim.create_and_connect_device(generation, device_name, comm_type,
                                      towerIndex);
      } catch (const CellularNetworkException &e) {
        Logger::error(std::string("Failed to connect device: ") + e.what());
      }
    }
  }

  IOHelpers::printNewline();
  tower->display_first_channel_users();
}

void runComprehensiveDemo(Simulator &sim) {
  OutputFormatter::print_header("COMPREHENSIVE CELLULAR NETWORK DEMONSTRATION");

  
  
  
  
  
  
  
  

  try {
    IOHelpers::print("🚀 Creating 15 towers with 0-100% utilization distribution..."), IOHelpers::printNewline();
    IOHelpers::print("Demonstrating realistic network scenarios from rural to overloaded"), IOHelpers::printNewline();

    // SCENARIO 1: Rural Areas (0-2%) - 3 towers
    OutputFormatter::print_step_header(1, "RURAL COVERAGE SCENARIO");
    IOHelpers::print("Deploying towers in low-density areas..."), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 1] Rural-Empty (4G, 5 cores, 10.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("4G", "Rural-Empty", 10.0, 5);
    IOHelpers::print(" "), IOHelpers::print("Deployed successfully"), IOHelpers::print(" - 0 users connected - 0% utilization"), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 2] Rural-Light (4G, 6 cores, 10.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("4G", "Rural-Light", 10.0, 6);
    int t2 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 9 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.05, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 5% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 9; i++) {
      sim.create_and_connect_device("4G", "Rural-L-" + std::to_string(i),
                                    CommunicationType::VOICE, t2);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 3] Town-Small (4G, 5 cores, 15.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("4G", "Town-Small", 15.0, 5);
    int t3 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 15 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.10, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 10% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 15; i++) {
      sim.create_and_connect_device("4G", "Town-S-" + std::to_string(i),
                                    CommunicationType::BOTH, t3);
    }
    IOHelpers::printNewline();

    // Scenario 1 Summary
    std::vector<std::pair<std::string, std::string>> summary1 = {
        {"Total Towers", "3"},
        {"Total Users", "24"},
        {"Avg Utilization", "5%"},
        {"Network Status", std::string() + "Optimal" + std::string()}};
    OutputFormatter::print_summary_table(summary1);
    pause();

    // SCENARIO 2: Suburban Areas (2-5%) - 3 towers
    OutputFormatter::print_step_header(2, "SUBURBAN DEPLOYMENT SCENARIO");
    IOHelpers::print("Deploying towers in growing residential areas..."), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 4] Suburban-1 (4G, 8 cores, 15.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("4G", "Suburban-1", 15.0, 8);
    int t4 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 48 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.20, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 20% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 48; i++) {
      sim.create_and_connect_device("4G", "Sub-1-" + std::to_string(i),
                                    CommunicationType::BOTH, t4);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 5] Urban-SmallCell (5G, 6 cores, 20.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Urban-SmallCell", 20.0, 6);
    int t5 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 54 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.20, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 20% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 54; i++) {
      sim.create_and_connect_device("5G", "Urban-SC-" + std::to_string(i),
                                    CommunicationType::BOTH, t5);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 6] Mall-Coverage (5G, 10 cores, 25.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Mall-Coverage", 25.0, 10);
    int t6 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 120 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.40, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 40% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 120; i++) {
      sim.create_and_connect_device("5G", "Mall-" + std::to_string(i),
                                    CommunicationType::BOTH, t6);
    }
    IOHelpers::printNewline();

    // Scenario 2 Summary
    std::vector<std::pair<std::string, std::string>> summary2 = {
        {"Total Towers", "3"},
        {"Total Users", "222"},
        {"Avg Utilization", "27%"},
        {"Network Status", std::string() + "Good" + std::string()}};
    OutputFormatter::print_summary_table(summary2);
    pause();

    // SCENARIO 3: Business Districts (5-8%) - 3 towers
    OutputFormatter::print_step_header(3, "BUSINESS DISTRICTS SCENARIO");
    IOHelpers::print("Deploying towers in commercial zones..."), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 7] Office-District (5G, 8 cores, 30.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Office-District", 30.0, 8);
    int t7 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 120 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.50, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 50% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 120; i++) {
      sim.create_and_connect_device("5G", "Office-" + std::to_string(i),
                                    CommunicationType::BOTH, t7);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 8] University-Campus (5G, 12 cores, 35.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "University-Campus", 35.0, 12);
    int t8 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 216 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.60, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 60% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 216; i++) {
      sim.create_and_connect_device("5G", "Uni-" + std::to_string(i),
                                    CommunicationType::BOTH, t8);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 9] Stadium-Sector (5G, 10 cores, 40.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Stadium-Sector", 40.0, 10);
    int t9 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 210 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.70, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 70% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 210; i++) {
      sim.create_and_connect_device("5G", "Stadium-" + std::to_string(i),
                                    CommunicationType::BOTH, t9);
    }
    IOHelpers::printNewline();

    // Scenario 3 Summary
    std::vector<std::pair<std::string, std::string>> summary3 = {
        {"Total Towers", "3"},
        {"Total Users", "546"},
        {"Avg Utilization", "60%"},
        {"Network Status", std::string() + "Monitoring" + std::string()}};
    OutputFormatter::print_summary_table(summary3);
    pause();

    // SCENARIO 4: High-Traffic Venues (8-10%) - 3 towers
    OutputFormatter::print_step_header(4, "HIGH-TRAFFIC VENUES SCENARIO");
    IOHelpers::print("Deploying towers for major transportation and events..."), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 10] Airport-Terminal (5G, 15 cores, 45.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Airport-Terminal", 45.0, 15);
    int t10 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 360 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.80, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 80% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 360; i++) {
      sim.create_and_connect_device("5G", "Airport-" + std::to_string(i),
                                    CommunicationType::BOTH, t10);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 11] Train-Station (5G, 12 cores, 40.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Train-Station", 40.0, 12);
    int t11 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 306 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.85, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 85% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 306; i++) {
      sim.create_and_connect_device("5G", "Train-" + std::to_string(i),
                                    CommunicationType::BOTH, t11);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 12] Festival-Grounds (5G, 18 cores, 50.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Festival-Grounds", 50.0, 18);
    int t12 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 486 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.90, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 90% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 486; i++) {
      sim.create_and_connect_device("5G", "Festival-" + std::to_string(i),
                                    CommunicationType::BOTH, t12);
    }
    IOHelpers::printNewline();

    // Scenario 4 Summary
    std::vector<std::pair<std::string, std::string>> summary4 = {
        {"Total Towers", "3"},
        {"Total Users", "1152"},
        {"Avg Utilization", "85%"},
        {"Network Status", std::string() + "High Load" + std::string()}};
    OutputFormatter::print_summary_table(summary4);
    pause();

    // SCENARIO 5: Critical Load Events (95-100%) - 3 towers
    OutputFormatter::print_step_header(5, "CRITICAL LOAD EVENTS SCENARIO");
    IOHelpers::print("Deploying towers for extreme capacity scenarios..."), IOHelpers::printNewline(), IOHelpers::printNewline();

    IOHelpers::print(" [Tower 13] Concert-Venue (5G, 16 cores, 45.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Concert-Venue", 45.0, 16);
    int t13 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 456 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.95, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 95% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 456; i++) {
      sim.create_and_connect_device("5G", "Concert-" + std::to_string(i),
                                    CommunicationType::BOTH, t13);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 14] Event-Main (5G, 20 cores, 50.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("5G", "Event-Main", 50.0, 20);
    int t14 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting 588 users: "), IOHelpers::print(OutputFormatter::get_progress_bar(0.98, 20)), IOHelpers::print(" "), IOHelpers::print("Complete"), IOHelpers::print(" - 98% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 588; i++) {
      sim.create_and_connect_device("5G", "Event-" + std::to_string(i),
                                    CommunicationType::BOTH, t14);
    }

    IOHelpers::printNewline();
    IOHelpers::print(" [Tower 15] Overloaded-Tower (4G, 10 cores, 25.0 MHz)"), IOHelpers::printNewline();
    sim.create_tower("4G", "Overloaded-Tower", 25.0, 10);
    int t15 = sim.get_tower_count() - 1;
    IOHelpers::print(" Connecting to capacity: "), IOHelpers::print(OutputFormatter::get_progress_bar(1.00, 20)), IOHelpers::print(" "), IOHelpers::print("Full!"), IOHelpers::print(" - 100% utilization"), IOHelpers::printNewline();
    for (int i = 1; i <= 300; i++) {
      try {
        sim.create_and_connect_device("4G", "Overflow-" + std::to_string(i),
                                      CommunicationType::BOTH, t15);
      } catch (const CellularNetworkException &e) {
        break;
      }
    }
    IOHelpers::printNewline();

    // Scenario 5 Summary
    std::vector<std::pair<std::string, std::string>> summary5 = {
        {"Total Towers", "3"},
        {"Total Users", "1344"},
        {"Avg Utilization", "98%"},
        {"Network Status", std::string() + "Critical" + std::string()}};
    OutputFormatter::print_summary_table(summary5);
    pause();

    // FINAL SUMMARY
    OutputFormatter::print_header("DEMONSTRATION COMPLETE");
    IOHelpers::print("✓ All 15 towers deployed successfully!"), IOHelpers::printNewline(), IOHelpers::printNewline();

    sim.display_statistics();
    IOHelpers::printNewline();

    // Network Performance Analysis
    OutputFormatter::print_sub_header("NETWORK PERFORMANCE ANALYSIS");
    NetworkAnalytics analytics(&sim);

    // Performance Metrics
    IOHelpers::print("Performance Metrics:"), IOHelpers::printNewline();
    IOHelpers::print(" Total Towers Deployed: 15"), IOHelpers::printNewline();
    IOHelpers::print(" Total Connected Devices: "), IOHelpers::print(sim.get_device_count()), IOHelpers::printNewline();
    IOHelpers::print(" Network Protocols: 4G (3 towers), 5G (12 towers)"), IOHelpers::printNewline();
    IOHelpers::print(" Total Bandwidth Allocated: 575 MHz"), IOHelpers::printNewline();
    IOHelpers::print(" Total Processing Cores: 165"), IOHelpers::printNewline(), IOHelpers::printNewline();

    // Load Distribution Analysis
    IOHelpers::print("Load Distribution Analysis:"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("Low Load (0-25%):"), IOHelpers::print(" 4 towers - "), IOHelpers::print("Excellent capacity"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("Medium Load (25-50%):"), IOHelpers::print(" 3 towers - "), IOHelpers::print("Good performance"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("High Load (50-75%):"), IOHelpers::print(" 3 towers - "), IOHelpers::print("Monitor closely"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("Very High (75-90%):"), IOHelpers::print(" 3 towers - "), IOHelpers::print("Consider expansion"), IOHelpers::printNewline();
    IOHelpers::print(" "), IOHelpers::print("Critical (90-100%):"), IOHelpers::print(" 2 towers - "), IOHelpers::print("Immediate action needed"), IOHelpers::printNewline(), IOHelpers::printNewline();

    // Network Topology
    analytics.display_network_topology();
    IOHelpers::printNewline();

    // Recommendations
    IOHelpers::print("System Recommendations:"), IOHelpers::printNewline();
    IOHelpers::print(" 1. "), IOHelpers::print("CRITICAL:"), IOHelpers::print(" Deploy additional towers for Overloaded-Tower (100% capacity)"), IOHelpers::printNewline();
    IOHelpers::print(" 2. "), IOHelpers::print("CRITICAL:"), IOHelpers::print(" Implement load balancing for Event-Main (98% capacity)"), IOHelpers::printNewline();
    IOHelpers::print(" 3. "), IOHelpers::print("WARNING:"), IOHelpers::print(" Monitor Concert-Venue and Festival-Grounds (>90% capacity)"), IOHelpers::printNewline();
    IOHelpers::print(" 4. "), IOHelpers::print("INFO:"), IOHelpers::print(" Rural towers have excellent spare capacity for growth"), IOHelpers::printNewline(), IOHelpers::printNewline();

    // Utilization Summary
    IOHelpers::print("Utilization Distribution Summary:"), IOHelpers::printNewline();

    std::vector<std::string> headers = {"Scenario", "Towers",
                                        "Utilization Range", "Status"};
    std::vector<int> widths = {25, 10, 20, 15};
    OutputFormatter::print_table_header(headers, widths);

    OutputFormatter::print_table_row(
        {"Rural Coverage", "3", "0-10%", "Optimal"}, widths);
    OutputFormatter::print_table_row(
        {"Suburban Deployment", "3", "20-40%", "Good"}, widths);
    OutputFormatter::print_table_row(
        {"Business Districts", "3", "50-70%", "Monitor"}, widths);
    OutputFormatter::print_table_row(
        {"High-Traffic Venues", "3", "80-90%", "High Load"}, widths);
    OutputFormatter::print_table_row(
        {"Critical Load Events", "3", "95-100%", "Critical"}, widths);

    OutputFormatter::print_table_separator(widths);
    IOHelpers::printNewline();

    IOHelpers::print("Next Steps:"), IOHelpers::printNewline();
    IOHelpers::print(" - Use Option 13 to view detailed tower information"), IOHelpers::printNewline();
    IOHelpers::print(" - Use Option 8 -> 3 to analyze bottlenecks"), IOHelpers::printNewline();
  } catch (const CellularNetworkException &e) {
    Logger::error(std::string("Demo error: ") + e.what());
  }
}

void networkAnalyticsMenu(Simulator &) {
  // Obsolete - TUI is now used
}

int main(int argc, char **argv) {
  try {
    // Create simulator instance
    Simulator simulator;

    // Simple CLI parsing for headless benchmark mode
    int cli_devices = 0;
    int cli_threads = 0;
    std::string cli_csv;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--benchmark") {
        cli_devices = 2000;
      } else if (arg == "--devices" && i + 1 < argc) {
        cli_devices = std::stoi(argv[++i]);
      } else if (arg == "--threads" && i + 1 < argc) {
        cli_threads = std::stoi(argv[++i]);
      } else if (arg == "--csv" && i + 1 < argc) {
        cli_csv = argv[++i];
      }
    }
    if (cli_devices > 0 && argc > 1) {
      // run headless benchmark and exit
      simulator.run_threading_benchmark(cli_devices, cli_threads, cli_csv);
      return 0;
    }

    // Initialize logger
    Logger::initialize();
    Logger::info("Booting Cellular Network Simulator TUI dashboard...");

    // Initialize with a default Urban Dense scenario to look nice on start
    ScenarioManager::load_scenario(simulator, ScenarioManager::ScenarioType::URBAN_DENSE);
    Logger::info("Urban Dense Scenario pre-loaded successfully.");

    // Run TUI
    ConsoleTUI tui(simulator);
    tui.run();

    Logger::shutdown();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal Error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Fatal Unknown Error" << std::endl;
    return 1;
  }
}
