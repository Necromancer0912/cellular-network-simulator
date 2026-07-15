#ifndef CELLULARCORE_H
#define CELLULARCORE_H

#include "Protocol.h"
#include "UserDevice.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

/**
 * Cellular Core class that manages message generation and device limits
 * Demonstrates data hiding and encapsulation
 */
class CellularCore {
private:
  static std::atomic<int> core_counter;
  int core_id;
  std::shared_ptr<Protocol> protocol;
  int max_devices;
  int current_device_count;
  long long total_messages_generated;
  mutable std::mutex core_mutex; // Mutex for thread safety

public:
  // Constructors
  CellularCore(std::shared_ptr<Protocol> proto);
  CellularCore(const CellularCore &other);
  virtual ~CellularCore();

  // Assignment operator
  CellularCore &operator=(const CellularCore &other);

  // Getters
  int get_core_id() const { return core_id; }
  int get_max_devices() const { return max_devices; }
  int get_current_device_count() const { return current_device_count; }
  long long get_total_messages_generated() const { return total_messages_generated; }
  std::shared_ptr<Protocol> get_protocol() const { return protocol; }

  // Core functionality
  void calculate_max_devices(double total_bandwidth_mhz);
  bool can_accommodate_device(const UserDevice &device);
  void register_device(const UserDevice &device);
  void unregister_device();
  void generate_messages(int count);

  // Utility
  void display_core_info() const;
  double get_utilization() const;

  // Static method
  static int get_total_cores() { return core_counter; }
};

#endif // CELLULARCORE_H
