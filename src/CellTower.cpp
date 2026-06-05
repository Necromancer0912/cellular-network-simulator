#include "CellTower.h"
#include "Exceptions.h"
#include "Utils.h"
#include "IOHelpers.h"
#include <algorithm>
#include <cmath>

// Initialize static member
int CellTower::tower_counter = 0;

// FrequencyChannel implementation
FrequencyChannel::FrequencyChannel(int id, double start, double end, int max,
                                   int antenna)
    : channel_id(id), start_frequency(start), end_frequency(end),
      max_users(max), antenna_id(antenna) {}

bool FrequencyChannel::is_full() const {
  return connected_devices.size() >= static_cast<size_t>(max_users);
}

int FrequencyChannel::get_available_slots() const {
  return max_users - connected_devices.size();
}

// CellTower implementation
CellTower::CellTower(const std::string &loc, std::shared_ptr<Protocol> proto,
                     double bandwidthMHz, int num_cores)
    : location(loc), protocol(proto), total_bandwidth_mhz(bandwidthMHz),
  max_supported_devices(0), beamforming_multiplier(1.0), x(0.0), y(0.0) {

  tower_id = ++tower_counter;

  if (!protocol) {
    throw ConfigurationException("Protocol cannot be null for cell tower");
  }

  // Create cellular cores
  for (int i = 0; i < num_cores; i++) {
    auto core = std::make_shared<CellularCore>(protocol);
    core->calculate_max_devices(bandwidthMHz);
    cores.push_back(core);
  }

  // Initialize frequency channels
  initialize_channels();

  // Calculate realistic max supported devices
  int users_per_channel = protocol->get_users_per_channel();
  int channel_limit = channels.size() * users_per_channel;
  
  // Core limit (sum of all cores)
  long long core_limit = 0;
  for (const auto &core : cores) {
    core_limit += static_cast<long long>(core->get_max_devices());
    if (core_limit > static_cast<long long>(std::numeric_limits<int>::max())) {
      throw CapacityException("Computed core device limit exceeds integer range");
    }
  }

  // The actual limit is the minimum of what channels can hold and what cores can process
  max_supported_devices = static_cast<int>(std::min(static_cast<long long>(channel_limit), core_limit));
}

CellTower::CellTower(const CellTower &other)
    : location(other.location), protocol(other.protocol),
      total_bandwidth_mhz(other.total_bandwidth_mhz),
      max_supported_devices(other.max_supported_devices),
      beamforming_multiplier(other.beamforming_multiplier),
      x(other.x), y(other.y) {

  tower_id = ++tower_counter;

  // Deep copy cores
  for (const auto &core : other.cores) {
    cores.push_back(std::make_shared<CellularCore>(*core));
  }

  // Initialize fresh channels
  initialize_channels();
}

CellTower::~CellTower() {
  // Destructor
}

CellTower &CellTower::operator=(const CellTower &other) {
  if (this != &other) {
    location = other.location;
    protocol = other.protocol;
    total_bandwidth_mhz = other.total_bandwidth_mhz;
    max_supported_devices = other.max_supported_devices;
    beamforming_multiplier = other.beamforming_multiplier;
    x = other.x;
    y = other.y;

    cores.clear();
    for (const auto &core : other.cores) {
      cores.push_back(std::make_shared<CellularCore>(*core));
    }

    connected_devices.clear();
    initialize_channels();
  }
  return *this;
}

void CellTower::initialize_channels() {
  channels.clear();

  double channelBW = protocol->get_channel_bandwidth();
  int users_per_channel = protocol->get_users_per_channel();
  // adjust for beamforming
  int adjusted_users = std::max(1, static_cast<int>(std::round(users_per_channel * beamforming_multiplier)));
  int totalChannels = protocol->calculate_total_channels(total_bandwidth_mhz);

  // Check if MIMO protocol
  std::string protoName = protocol->get_protocol_name();
  int numAntennas = 1;

  if (protoName.find("4G") != std::string::npos) {
    numAntennas = 4;
  } else if (protoName.find("5G") != std::string::npos) {
    numAntennas = 16;
  }

  int channel_id = 0;
  for (int antenna = 0; antenna < numAntennas; antenna++) {
    for (int i = 0; i < totalChannels; i++) {
      double startFreq = i * channelBW;
      double endFreq = startFreq + channelBW;
      channels.emplace_back(channel_id++, startFreq, endFreq, adjusted_users,
                            antenna);
    }
  }
}

void CellTower::apply_beamforming(double factor) {
  std::lock_guard<std::mutex> lock(tower_mutex);
  if (factor <= 0.0) return;
  beamforming_multiplier = factor;
  // reinitialize channels to apply new capacity
  initialize_channels();
  // recalc max_supported_devices with beamforming applied to BOTH channels AND cores
  int users_per_channel = protocol->get_users_per_channel();
  long long channel_limit = static_cast<long long>(channels.size()) * static_cast<long long>(users_per_channel * beamforming_multiplier);
  long long core_limit = 0;
  for (const auto &core : cores) {
    core_limit += static_cast<long long>(core->get_max_devices());
  }
  // Apply beamforming to core limit as well (better resource utilization)
  core_limit = static_cast<long long>(core_limit * beamforming_multiplier);
  max_supported_devices = static_cast<int>(std::min(channel_limit, core_limit));
}

void CellTower::disable_beamforming() {
  std::lock_guard<std::mutex> lock(tower_mutex);
  beamforming_multiplier = 1.0;
  initialize_channels();
  // recalc max_supported_devices
  int users_per_channel = protocol->get_users_per_channel();
  long long channel_limit = static_cast<long long>(channels.size()) * static_cast<long long>(users_per_channel);
  long long core_limit = 0;
  for (const auto &core : cores) {
    core_limit += static_cast<long long>(core->get_max_devices());
  }
  max_supported_devices = static_cast<int>(std::min(channel_limit, core_limit));
}

int CellTower::find_available_channel() const {
  for (size_t i = 0; i < channels.size(); i++) {
    if (!channels[i].is_full()) {
      return i;
    }
  }
  return -1; // No available channel
}

bool CellTower::connect_device(std::shared_ptr<UserDevice> device) {
  std::lock_guard<std::mutex> lock(tower_mutex);
  if (!device) {
    throw ConfigurationException("Cannot connect null device");
  }

  // Check if device is already connected
  if (connected_devices.find(device->get_device_id()) !=
      connected_devices.end()) {
    throw ProtocolException("Device already connected to this tower");
  }

  // Check if tower can accommodate more devices
  if (connected_devices.size() >= static_cast<size_t>(max_supported_devices)) {
    throw CapacityException("Tower has reached maximum capacity (" + 
                          std::to_string(max_supported_devices) + " devices)");
  }

  // Find available channel
  int channelIdx = find_available_channel();
  if (channelIdx < 0) {
    throw FrequencyException("No available frequency channels");
  }

  // Find available core
  std::shared_ptr<CellularCore> availableCore = nullptr;
  for (auto &core : cores) {
    if (core->can_accommodate_device(*device)) {
      availableCore = core;
      break;
    }
  }

  if (!availableCore) {
    throw CapacityException("No available cellular core with capacity");
  }

  // Connect device
  device->set_connection_status(true);
  device->set_assigned_channel(channels[channelIdx].channel_id);
  device->set_assigned_antenna(channels[channelIdx].antenna_id);

  channels[channelIdx].connected_devices.push_back(device);
  connected_devices[device->get_device_id()] = device;
  availableCore->register_device(*device);

  return true;
}

bool CellTower::disconnect_device(int device_id) {
  std::lock_guard<std::mutex> lock(tower_mutex);
  auto it = connected_devices.find(device_id);
  if (it == connected_devices.end()) {
    return false;
  }

  auto device = it->second;
  device->set_connection_status(false);

  // Remove from channel
  int channel_id = device->get_assigned_channel();
  for (auto &channel : channels) {
    if (channel.channel_id == channel_id) {
      auto &devices = channel.connected_devices;
      devices.erase(
          std::remove_if(devices.begin(), devices.end(),
                         [device_id](const std::shared_ptr<UserDevice> &d) {
                           return d->get_device_id() == device_id;
                         }),
          devices.end());
      break;
    }
  }

  connected_devices.erase(it);

  // Unregister from core
  for (auto &core : cores) {
    if (core->get_current_device_count() > 0) {
      core->unregister_device();
      break;
    }
  }

  return true;
}

std::shared_ptr<UserDevice> CellTower::get_device(int device_id) const {
  std::lock_guard<std::mutex> lock(tower_mutex);
  auto it = connected_devices.find(device_id);
  if (it != connected_devices.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::shared_ptr<UserDevice>> CellTower::get_all_devices() const {
  std::vector<std::shared_ptr<UserDevice>> devices;
  for (const auto &pair : connected_devices) {
    devices.push_back(pair.second);
  }
  return devices;
}

std::vector<std::shared_ptr<UserDevice>>
CellTower::get_users_on_channel(int channel_id) const {
  for (const auto &channel : channels) {
    if (channel.channel_id == channel_id) {
      return channel.connected_devices;
    }
  }
  return std::vector<std::shared_ptr<UserDevice>>();
}

std::vector<std::shared_ptr<UserDevice>>
CellTower::get_users_on_first_channel() const {
  if (channels.empty()) {
    return std::vector<std::shared_ptr<UserDevice>>();
  }
  return channels[0].connected_devices;
}

const FrequencyChannel &CellTower::get_channel(int channel_id) const {
  for (const auto &channel : channels) {
    if (channel.channel_id == channel_id) {
      return channel;
    }
  }
  throw FrequencyException("Channel not found: " + std::to_string(channel_id));
}

void CellTower::display_tower_info() const {
  OutputFormatter::print_sub_header("Cell Tower Information");
  
  IOHelpers::print("  Tower ID: ");
  IOHelpers::printInt(tower_id);
  IOHelpers::printNewline();
  
  IOHelpers::print("  Location: ");
  IOHelpers::println(location.c_str());
  
  IOHelpers::print("  Protocol: ");
  IOHelpers::println(protocol->get_protocol_name().c_str());
  
  IOHelpers::print("  Total Bandwidth: ");
  IOHelpers::printDouble(total_bandwidth_mhz, 1);
  IOHelpers::println(" MHz");
  
  IOHelpers::print("  Max Devices: ");
  IOHelpers::printInt(max_supported_devices);
  IOHelpers::printNewline();
  
  IOHelpers::print("  Connected Devices: ");
  IOHelpers::printInt(connected_devices.size());
  IOHelpers::printNewline();
  
  IOHelpers::print("  Total Channels: ");
  IOHelpers::printInt(channels.size());
  IOHelpers::printNewline();
  
  IOHelpers::print("  Cellular Cores: ");
  IOHelpers::printInt(cores.size());
  IOHelpers::printNewline();

  double util = get_utilization();
  IOHelpers::print("  Utilization: ");
  IOHelpers::print(OutputFormatter::get_progress_bar(util / 100.0).c_str());
  IOHelpers::print(" ");
  IOHelpers::printDouble(util, 1);
  IOHelpers::println("%");
}

void CellTower::display_channel_info() const {
  OutputFormatter::print_sub_header("Channel Information");

  std::vector<std::string> headers = {"Channel", "Frequency", "Antenna",
                                      "Users", "Capacity"};
  std::vector<int> widths = {10, 20, 10, 10, 15};

  OutputFormatter::print_table_header(headers, widths);

  for (const auto &channel : channels) {
    std::vector<std::string> row;
    row.push_back(std::to_string(channel.channel_id));

    std::string freqRange =
        std::to_string(static_cast<int>(channel.start_frequency)) + "-" +
        std::to_string(static_cast<int>(channel.end_frequency)) + " kHz";
    row.push_back(freqRange);
    row.push_back(std::to_string(channel.antenna_id));
    row.push_back(std::to_string(channel.connected_devices.size()));
    row.push_back(std::to_string(channel.connected_devices.size()) + "/" +
                  std::to_string(channel.max_users));

    OutputFormatter::print_table_row(row, widths);
  }
}

void CellTower::display_first_channel_users() const {
  OutputFormatter::print_sub_header("First Channel Users");

  auto users = get_users_on_first_channel();
  if (users.empty()) {
    IOHelpers::println("  No users on first channel");
    return;
  }

  // header with total count
  double startFreq = channels[0].start_frequency;
  double endFreq = channels[0].end_frequency;
  
  IOHelpers::print("\n  Channel 0 Status (");
  IOHelpers::printDouble(startFreq, 0);
  IOHelpers::print("-");
  IOHelpers::printDouble(endFreq, 0);
  IOHelpers::println(" kHz)");
  
  IOHelpers::print("  Total Devices: ");
  IOHelpers::printInt(users.size());
  IOHelpers::printNewline();
  IOHelpers::println("  Device List:");
  IOHelpers::printNewline();

  // display devices in a grid layout (4 columns)
  const int cols = 4;

  for (size_t i = 0; i < users.size(); i++) {
    const auto &user = users[i];

    // start new row
    if (i % cols == 0) {
      IOHelpers::print("     ");
    }

    // format: [ID] Gen
    IOHelpers::print("[");
    char idBuf[8];
    IOHelpers::intToString(user->get_device_id(), idBuf, 8);
    if (user->get_device_id() < 10) IOHelpers::print(" ");
    IOHelpers::print(idBuf);
    IOHelpers::print("] ");
    IOHelpers::print(user->get_device_type().c_str());

    // add spacing or newline
    if ((i + 1) % cols == 0 || i == users.size() - 1) {
      IOHelpers::printNewline();
    } else {
      IOHelpers::print("  ");
    }
  }

  IOHelpers::printNewline();
}

double CellTower::get_utilization() const {
  if (max_supported_devices == 0)
    return 0.0;
  return (static_cast<double>(connected_devices.size()) /
          max_supported_devices) *
         100.0;
}
