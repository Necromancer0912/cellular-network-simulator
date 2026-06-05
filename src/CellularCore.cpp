#include "CellularCore.h"
#include "Exceptions.h"
#include "IOHelpers.h"
#include <climits>

// Initialize static member
int CellularCore::core_counter = 0;

CellularCore::CellularCore(std::shared_ptr<Protocol> proto)
    : protocol(proto), max_devices(0), current_device_count(0),
      total_messages_generated(0) {
  core_id = ++core_counter;
}

CellularCore::CellularCore(const CellularCore &other)
    : protocol(other.protocol), max_devices(other.max_devices),
      current_device_count(0), total_messages_generated(0) {
  core_id = ++core_counter;
}

CellularCore::~CellularCore() {
  // Destructor
}

CellularCore &CellularCore::operator=(const CellularCore &other) {
  if (this != &other) {
    protocol = other.protocol;
    max_devices = other.max_devices;
    // Don't copy counts - each core maintains its own
  }
  return *this;
}

void CellularCore::calculate_max_devices(double /* total_bandwidth_mhz */) {
  if (!protocol) {
    throw ConfigurationException("Protocol not set for cellular core");
  }

  int messages_per_user = protocol->calculate_messages_per_user();
  // Core capacity: 1000 messages considering overhead
  int overhead = protocol->get_overhead();
  int effectiveCapacity = 1000 - (1000 / 100) * overhead;

  if (messages_per_user <= 0) {
    throw ConfigurationException("Invalid messages per user in core capacity calculation");
  }
  max_devices = effectiveCapacity / messages_per_user;
}

bool CellularCore::can_accommodate_device(const UserDevice & /* device */) {
  return current_device_count < max_devices;
}

void CellularCore::register_device(const UserDevice &device) {
  std::lock_guard<std::mutex> lock(core_mutex);
  if (current_device_count >= max_devices) {
    throw CapacityException("Core " + std::to_string(core_id) +
                            " is at full capacity");
  }
  current_device_count++;
  long long add = static_cast<long long>(device.get_message_count());
  if (total_messages_generated > LLONG_MAX - add) {
    throw CapacityException("Message counter overflow in core " + std::to_string(core_id));
  }
  total_messages_generated += add;
}

void CellularCore::unregister_device() {
  std::lock_guard<std::mutex> lock(core_mutex);
  if (current_device_count > 0) {
    current_device_count--;
  }
}

void CellularCore::generate_messages(int count) {
  std::lock_guard<std::mutex> lock(core_mutex);
  long long add = static_cast<long long>(count);
  if (total_messages_generated > LLONG_MAX - add) {
    throw CapacityException("Message counter overflow in core " + std::to_string(core_id));
  }
  total_messages_generated += add;
}

void CellularCore::display_core_info() const {
  double util = get_utilization();

  IOHelpers::print("    Core ID: ");
  IOHelpers::printInt(core_id);
  IOHelpers::printNewline();
  
  IOHelpers::print("    Protocol: ");
  IOHelpers::println(protocol ? protocol->get_protocol_name().c_str() : "None");
  
  IOHelpers::print("    Max Devices: ");
  IOHelpers::printInt(max_devices);
  IOHelpers::printNewline();
  
  IOHelpers::print("    Current Devices: ");
  IOHelpers::printInt(current_device_count);
  IOHelpers::printNewline();
  
  IOHelpers::print("    Total Messages: ");
  IOHelpers::printInt(total_messages_generated);
  IOHelpers::printNewline();
  
  IOHelpers::print("    Utilization: ");
  IOHelpers::printDouble(util, 2);
  IOHelpers::println("%");
}

double CellularCore::get_utilization() const {
  if (max_devices == 0)
    return 0.0;
  return (static_cast<double>(current_device_count) / max_devices) * 100.0;
}
