#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "CellTower.h"
#include "Protocol.h"
#include "UserDevice.h"
#include "Utils.h"
#include <future>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

/**
 * Template class for managing collections of network elements
 */
template <typename T> class NetworkCollection {
private:
  std::vector<std::shared_ptr<T>> elements;
  std::string collection_name;

public:
  NetworkCollection(const std::string &name) : collection_name(name) {}

  void add(std::shared_ptr<T> element) { elements.push_back(element); }

  void remove(int index) {
    if (index >= 0 && index < elements.size()) {
      elements.erase(elements.begin() + index);
    }
  }

  std::shared_ptr<T> get(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < elements.size()) {
      return elements[index];
    }
    return nullptr;
  }

  int size() const { return elements.size(); }

  std::vector<std::shared_ptr<T>> get_all() const { return elements; }

  void clear() { elements.clear(); }

  std::string get_name() const { return collection_name; }
};

struct Packet {
  int id;
  int source_id;
  int dest_id;
  double progress;      // 0.0 to 1.0 along current hop
  int current_hop;      // 0: Dev->SrcTower, 1: SrcTower->Router, 2: Router->DstTower, 3: DstTower->DstDev
  int src_tower_idx;
  int dst_tower_idx;
  std::string type;     // "VOICE" (blue) or "DATA" (green)
};

/**
 * Main Simulator class that orchestrates the cellular network simulation
 */
class Simulator {
private:
  NetworkCollection<CellTower> towers;
  NetworkCollection<UserDevice> devices;
  std::map<std::string, std::shared_ptr<Protocol>> available_protocols;
  
  // Thread Pool for parallel generation
  ThreadPool threadPool;

  // RF and packet routing states
  std::vector<std::vector<bool>> obstacles; // 100x100 grid representing 10mx10m cells
  std::vector<Packet> active_packets;
  int next_packet_id;
  int dropped_packets;
  int processed_packets;
  int total_packets_routed;
  std::vector<std::string> handover_logs;

  // Private helper methods
  void update_packet_routing();
  void trigger_packet_generation();

  // Statistics
  struct Statistics {
    std::atomic<long long> total_connections;
    std::atomic<long long> failed_connections;
    std::atomic<long long> total_messages;
    std::atomic<double> average_utilization;

    Statistics()
        : total_connections(0), failed_connections(0), total_messages(0),
          average_utilization(0.0) {}
  } stats;

  // Private helper methods
  void initialize_protocols();
  std::shared_ptr<UserDevice> create_device(const std::string &generation,
                                            const std::string &name,
                                            CommunicationType type);

public:
  // Constructor and destructor
  Simulator();
  ~Simulator();

  // Tower management
  void create_tower(const std::string &generation, const std::string &location,
                    double bandwidthMHz, int num_cores = 1);
  std::shared_ptr<CellTower> get_tower(int index) const;
  int get_tower_count() const { return towers.size(); }

  // Device management
  void create_and_connect_device(const std::string &generation,
                                 const std::string &name,
                                 CommunicationType type, int towerIndex);

  // Async device connection
  void connect_devices_async(const std::string &generation, int count,
                             int towerIndex);
                             
  // Parallel network generation (using ThreadPool)
  void generate_network_parallel(const std::string &generation, int count, int towerIndex);

  int get_device_count() const { return devices.size(); }
  std::shared_ptr<UserDevice> get_device(int index) const { return devices.get(index); }
  long long get_total_connections() const { return stats.total_connections.load(); }
  long long get_failed_connections() const { return stats.failed_connections.load(); }
  long long get_total_messages() const { return stats.total_messages.load(); }

  // Simulation operations
  void run_simulation(const std::string &generation);
  void display_all_towers() const;
  void display_all_devices() const;
  void display_statistics() const;

  // Interactive features
  void simulate_traffic_fluctuation();
  void optimize_network();

  // Protocol queries
  void display_protocol_info(const std::string &generation) const;
  std::shared_ptr<Protocol> get_protocol(const std::string &generation) const;

  // Specific generation analysis
  void analyze2G() const;
  void analyze3G() const;
  void analyze4G() const;
  void analyze5G() const;
  void analyze6G() const;
  void analyze7G() const;
  
  // Advanced Features
  void run_threading_benchmark();
  // Headless benchmark variant: devices, thread count (0 = auto), csv output path (optional)
  void run_threading_benchmark(int devices, int threads, const std::string &csvPath = "");

  // Enhanced benchmark modes
  enum class BenchmarkMode { MINIMAL = 0, GOOD = 1, WILD = 2 };
  void run_threading_benchmark(BenchmarkMode mode, int devices, int threads, const std::string &csvPath = "");

  // Live monitor
  void start_live_monitor(int interval_seconds = 5);
  void stop_live_monitor();
  bool is_monitor_running() const;
  
  // Beamforming control
  void apply_beamforming_to_tower(int towerIndex, double factor);
  void disable_beamforming_on_tower(int towerIndex);

  // Device Analytics
  void display_device_analytics() const;
  void display_device_breakdown_by_generation() const;
  void display_device_breakdown_by_tower() const;
  void display_device_breakdown_by_frequency() const;
  void display_device_breakdown_by_area() const;
  void display_total_devices_summary() const;

  // Reset simulation
  void reset();

  // RF Propagation, Raycasting and Obstacles
  bool check_line_of_sight(double x1, double y1, double x2, double y2) const;
  double compute_rssi(double distance, bool has_obstacle) const;
  bool has_obstacle_at(int gx, int gy) const;
  void toggle_obstacle_at(int gx, int gy);
  const std::vector<std::vector<bool>>& get_obstacles() const { return obstacles; }

  // Packet Routing Engine Queries
  const std::vector<Packet>& get_active_packets() const { return active_packets; }
  int get_dropped_packets() const { return dropped_packets; }
  int get_processed_packets() const { return processed_packets; }
  int get_total_packets_routed() const { return total_packets_routed; }
  const std::vector<std::string>& get_handover_logs() const { return handover_logs; }
  void clear_handover_logs() { handover_logs.clear(); }
};

#endif // SIMULATOR_H
