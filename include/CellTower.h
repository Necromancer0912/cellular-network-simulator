#ifndef CELLTOWER_H
#define CELLTOWER_H

#include "CellularCore.h"
#include "Protocol.h"
#include "UserDevice.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * Frequency Channel structure to track users per frequency
 */
struct FrequencyChannel {
  int channel_id;
  double start_frequency; // in kHz
  double end_frequency;   // in kHz
  int max_users;
  std::vector<std::shared_ptr<UserDevice>> connected_devices;
  int antenna_id; // For MIMO systems

  FrequencyChannel(int id, double start, double end, int max, int antenna = 0);
  bool is_full() const;
};

/**
 * Cell Tower class that manages user devices and frequency allocation
 * Demonstrates composition and aggregation
 */
class CellTower {
private:
  static std::atomic<int> tower_counter;
  int tower_id;
  std::string location;
  std::shared_ptr<Protocol> protocol;
  std::vector<std::shared_ptr<CellularCore>> cores;
  std::vector<FrequencyChannel> channels;
  std::map<int, std::shared_ptr<UserDevice>>
      connected_devices; // device_id -> device
  // Which core each connected device was registered to, so disconnect can
  // release capacity on the right core instead of guessing.
  std::map<int, std::shared_ptr<CellularCore>> device_core_map;
  double total_bandwidth_mhz;
  int max_supported_devices;
  double beamforming_multiplier;
  double x;
  double y;
  mutable std::mutex tower_mutex; // Mutex for thread safety

  // Private helper methods
  void initialize_channels();
  int find_available_channel() const;

public:
  // Constructors
  CellTower(const std::string &loc, std::shared_ptr<Protocol> proto,
            double bandwidthMHz, int num_cores = 1);
  CellTower(const CellTower &other);
  virtual ~CellTower();

  // Assignment operator
  CellTower &operator=(const CellTower &other);

  // Getters
  int get_tower_id() const { return tower_id; }
  std::string get_location() const { return location; }
  int get_max_supported_devices() const { return max_supported_devices; }
  int get_current_device_count() const {
    std::lock_guard<std::mutex> lock(tower_mutex);
    return static_cast<int>(connected_devices.size());
  }
  double get_total_bandwidth() const { return total_bandwidth_mhz; }
  std::shared_ptr<Protocol> get_protocol() const { return protocol; }
  int get_number_of_cores() const { return static_cast<int>(cores.size()); }
  const std::vector<std::shared_ptr<CellularCore>>& get_cores() const { return cores; }
  double get_x() const { return x; }
  double get_y() const { return y; }

  // Setters
  void set_position(double new_x, double new_y) { x = new_x; y = new_y; }

  // Device management
  bool connect_device(std::shared_ptr<UserDevice> device);
  bool disconnect_device(int device_id);
  std::shared_ptr<UserDevice> get_device(int device_id) const;
  std::vector<std::shared_ptr<UserDevice>> get_all_devices() const;

  // Channel management
  std::vector<std::shared_ptr<UserDevice>>
  get_users_on_channel(int channel_id) const;
  std::vector<std::shared_ptr<UserDevice>> get_users_on_first_channel() const;
  int get_total_channels() const { return channels.size(); }
  const FrequencyChannel &get_channel(int channel_id) const;

  // Statistics and display
  void display_tower_info() const;
  void display_channel_info() const;
  void display_first_channel_users() const;
  double get_utilization() const;

  // Beamforming (simulate improvement in users per channel)
  void apply_beamforming(double factor);
  void disable_beamforming();
  double get_beamforming_factor() const { return beamforming_multiplier; }

  // Static method
  static int get_total_towers() { return tower_counter; }
};

#endif // CELLTOWER_H
