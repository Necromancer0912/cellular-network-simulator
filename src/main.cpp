#include "Exceptions.h"
#include "NetworkAnalytics.h"
#include "Simulator.h"
#include "Utils.h"
#include "ConsoleTUI.h"
#include "IOHelpers.h"
#include <iostream>
#include <string>

namespace {

struct CliOptions {
  bool run_benchmark = false;
  int devices = 2000;
  int threads = 0; // 0 means "let Simulator pick hardware_concurrency()"
  std::string csv_path;
};

CliOptions parse_cli(int argc, char **argv) {
  CliOptions opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--benchmark") {
      opts.run_benchmark = true;
    } else if (arg == "--devices" && i + 1 < argc) {
      opts.run_benchmark = true;
      opts.devices = std::stoi(argv[++i]);
    } else if (arg == "--threads" && i + 1 < argc) {
      opts.threads = std::stoi(argv[++i]);
    } else if (arg == "--csv" && i + 1 < argc) {
      opts.csv_path = argv[++i];
    }
  }
  return opts;
}

} // namespace

int main(int argc, char **argv) {
  try {
    Simulator simulator;
    CliOptions opts = parse_cli(argc, argv);

    if (opts.run_benchmark) {
      simulator.run_threading_benchmark(opts.devices, opts.threads, opts.csv_path);
      return 0;
    }

    Logger::initialize();
    Logger::info("Booting Cellular Network Simulator TUI dashboard...");

    // Pre-load a scenario so the dashboard has real data on first frame.
    ScenarioManager::load_scenario(simulator, ScenarioManager::ScenarioType::URBAN_DENSE);
    Logger::info("Urban Dense Scenario pre-loaded successfully.");

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
