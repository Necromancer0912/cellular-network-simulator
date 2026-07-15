#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "CellTower.h"
#include "Protocol.h"
#include "UserDevice.h"
#include "Utils.h"
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * Template class for managing collections of network elements.
 *
 * Simulator hands this out to worker threads via generate_network_parallel
 * and connect_devices_async, so add()/get()/size() are called concurrently
 * from multiple threads during a parallel device-connect batch. An earlier
 * version was a plain vector wrapper with no locking, which meant concurrent
 * add() calls could race on a reallocation. The mutex below is what actually
 * makes this collection safe to share across threads, rather than just a
 * template-usage box to check.
 */
template <typename T> class NetworkCollection {
private:
  mutable std::mutex collection_mutex;
  std::vector<std::shared_ptr<T>> elements;
  std::string collection_name;

public:
  NetworkCollection(const std::string &name) : collection_name(name) {}

  void add(std::shared_ptr<T> element) {
    std::lock_guard<std::mutex> lock(collection_mutex);
    elements.push_back(element);
  }

  // Inserts a whole batch under a single lock acquisition. Prefer this over
  // repeated add() calls when a caller (e.g. a parallel provisioning task)
  // builds up a local batch of elements before publishing them - one lock
  // per batch instead of one lock per element removes most of the mutex
  // contention when several threads are each producing many elements at
  // once, without changing the fact that every element is still guarded by
  // the same mutex while it's actually inserted.
  void add_batch(const std::vector<std::shared_ptr<T>> &batch) {
    if (batch.empty()) return;
    std::lock_guard<std::mutex> lock(collection_mutex);
    elements.insert(elements.end(), batch.begin(), batch.end());
  }

  void remove(int index) {
    std::lock_guard<std::mutex> lock(collection_mutex);
    if (index >= 0 && static_cast<size_t>(index) < elements.size()) {
      elements.erase(elements.begin() + index);
    }
  }

  std::shared_ptr<T> get(int index) const {
    std::lock_guard<std::mutex> lock(collection_mutex);
    if (index >= 0 && static_cast<size_t>(index) < elements.size()) {
      return elements[index];
    }
    return nullptr;
  }

  int size() const {
    std::lock_guard<std::mutex> lock(collection_mutex);
    return static_cast<int>(elements.size());
  }

  std::vector<std::shared_ptr<T>> get_all() const {
    std::lock_guard<std::mutex> lock(collection_mutex);
    return elements;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(collection_mutex);
    elements.clear();
  }

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
  // Same connection logic as create_and_connect_device, but does not touch
  // the shared `devices` collection - the caller owns publishing the
  // returned device (individually via devices.add(), or in bulk via
  // devices.add_batch()). Used by run_threading_benchmark's parallel run so
  // several worker threads doing this concurrently only take the `devices`
  // lock once per batch instead of once per device; see the comment there
  // for why that distinction turned out to matter.
  std::shared_ptr<UserDevice> connect_device_deferred_registration(
      const std::string &generation, const std::string &name,
      CommunicationType type, int towerIndex);

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
  // devices: how many to provision; threads: worker count (0 = auto-detect
  // hardware_concurrency); csvPath: optional file to append machine-readable
  // results to. See src/Simulator.cpp for what this actually measures.
  void run_threading_benchmark(int devices, int threads, const std::string &csvPath = "");


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
