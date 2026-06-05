#include "Protocol.h"
#include "Exceptions.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>

// Protocol base class implementation
Protocol::Protocol(const std::string &name, int overhead, double bandwidth, int users)
    : protocol_name(name), overhead_per_100_messages(overhead),
      channel_bandwidth(bandwidth), users_per_channel(users) {}

Protocol::~Protocol() {}

int Protocol::calculate_total_channels(double total_bandwidth_mhz) const
{
    double totalBandwidthKHz = total_bandwidth_mhz * 1000.0;
    return static_cast<int>(totalBandwidthKHz / channel_bandwidth);
}

// TDMAProtocol implementation (2G)
TDMAProtocol::TDMAProtocol()
    : Protocol("TDMA (2G)", 20, 200.0, 16) {}

TDMAProtocol::~TDMAProtocol() {}

int TDMAProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    int totalChannels = calculate_total_channels(total_bandwidth_mhz);
    return totalChannels * users_per_channel;
}

int TDMAProtocol::calculate_messages_per_user() const
{
    // 5 messages for data + 15 messages for voice = 20 total
    return DATA_MESSAGES + VOICE_MESSAGES;
}

int TDMAProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    // Use 64-bit arithmetic and check for overflow
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }

    long long overhead = (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    long long effectiveMessages = total_messages + overhead;
    if (effectiveMessages < 0) {
        throw CapacityException("Invalid effective message count (negative)");
    }

    // Assuming each core can handle 1000 messages
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 1000.0));
}

std::string TDMAProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: Time Division Multiple Access" << std::endl;
    ss << "    Data Switching: Packet Switching" << std::endl;
    ss << "    Voice Switching: Circuit Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Channel Bandwidth: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Channel: " << users_per_channel << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User (Data): " << DATA_MESSAGES << std::endl;
    ss << "    Messages per User (Voice): " << VOICE_MESSAGES << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl;
    return ss.str();
}

// CDMAProtocol implementation (3G)
CDMAProtocol::CDMAProtocol()
    : Protocol("CDMA (3G)", 15, 200.0, 32) {}

CDMAProtocol::~CDMAProtocol() {}

int CDMAProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    int totalChannels = calculate_total_channels(total_bandwidth_mhz);
    return totalChannels * users_per_channel;
}

int CDMAProtocol::calculate_messages_per_user() const
{
    return MESSAGES_PER_USER;
}

int CDMAProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }
    long long effectiveMessages = total_messages + (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 1000.0));
}

std::string CDMAProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: Code Division Multiple Access" << std::endl;
    ss << "    Data Switching: Packet Switching" << std::endl;
    ss << "    Voice Switching: Packet Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Channel Bandwidth: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Channel: " << users_per_channel << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User: " << MESSAGES_PER_USER << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl;
    return ss.str();
}

// OFDMProtocol implementation (4G)
OFDMProtocol::OFDMProtocol()
    : Protocol("OFDM (4G)", 10, 10.0, 30) {}

OFDMProtocol::~OFDMProtocol() {}

int OFDMProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    int totalChannels = calculate_total_channels(total_bandwidth_mhz);
    // MIMO allows reuse of channels across antennas
    return totalChannels * users_per_channel * NUM_ANTENNAS;
}

int OFDMProtocol::calculate_messages_per_user() const
{
    return MESSAGES_PER_USER;
}

int OFDMProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }
    long long effectiveMessages = total_messages + (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 1000.0));
}

std::string OFDMProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: Orthogonal Frequency Division Multiplexing" << std::endl;
    ss << "    Switching: Packet Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Channel Bandwidth: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Channel: " << users_per_channel << std::endl;
    ss << "    MIMO Antennas: " << NUM_ANTENNAS << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User: " << MESSAGES_PER_USER << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl;
    ss << "    Channel reuse across " << NUM_ANTENNAS << " antennas" << std::endl;
    return ss.str();
}

// MassiveMIMOProtocol implementation (5G)
MassiveMIMOProtocol::MassiveMIMOProtocol()
    : Protocol("Massive MIMO (5G)", 8, 10.0, 30),
      high_frequency_band_mhz(10.0), users_per_high_freq_mhz(30) {}

MassiveMIMOProtocol::~MassiveMIMOProtocol() {}

int MassiveMIMOProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    // Base band (1 MHz at lower frequency)
    int baseBandChannels = calculate_total_channels(total_bandwidth_mhz);
    int baseBandUsers = baseBandChannels * users_per_channel;

    // High frequency band (10 MHz at 1800 MHz)
    long long highFreqUsers = static_cast<long long>(static_cast<int>(high_frequency_band_mhz)) * static_cast<long long>(users_per_high_freq_mhz);

    long long totalUsers = static_cast<long long>(baseBandUsers) + highFreqUsers;
    if (totalUsers < 0) {
        throw CapacityException("Negative user count computed for 5G max users");
    }
    long long totalWithMIMO = totalUsers * static_cast<long long>(NUM_ANTENNAS);
    if (totalWithMIMO > static_cast<long long>(std::numeric_limits<int>::max())) {
        throw CapacityException("Computed max users exceed integer limits for 5G");
    }
    return static_cast<int>(totalWithMIMO);
}

int MassiveMIMOProtocol::calculate_messages_per_user() const
{
    return MESSAGES_PER_USER;
}

int MassiveMIMOProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }
    long long effectiveMessages = total_messages + (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 1000.0));
}

std::string MassiveMIMOProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: OFDM with Massive MIMO" << std::endl;
    ss << "    Switching: Packet Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Base Band Channel: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Base Channel: " << users_per_channel << std::endl;
    ss << "    High Frequency Band: " << high_frequency_band_mhz << " MHz at 1800 MHz" << std::endl;
    ss << "    Users per High Freq MHz: " << users_per_high_freq_mhz << std::endl;
    ss << "    Massive MIMO Antennas: " << NUM_ANTENNAS << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User: " << MESSAGES_PER_USER << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl;
    ss << "    Channel reuse across " << NUM_ANTENNAS << " antennas" << std::endl;
    return ss.str();
}

// TerahertzProtocol implementation (6G)
TerahertzProtocol::TerahertzProtocol()
    : Protocol("Terahertz (6G)", 5, 5.0, 50),
      terahertz_band_mhz(100.0), users_per_terahertz_mhz(50),
      quantum_encryption(true), ai_beamforming(true) {}

TerahertzProtocol::~TerahertzProtocol() {}

int TerahertzProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    // Base band calculations
    int baseBandChannels = calculate_total_channels(total_bandwidth_mhz);
    int baseBandUsers = baseBandChannels * users_per_channel;

    // Terahertz band users (100 MHz at 300 GHz)
    int terahertzUsers = static_cast<int>(terahertz_band_mhz) * users_per_terahertz_mhz;

    // Ultra-massive MIMO with 64 antennas
    return (baseBandUsers + terahertzUsers) * NUM_ANTENNAS;
}

int TerahertzProtocol::calculate_messages_per_user() const
{
    return MESSAGES_PER_USER;
}

int TerahertzProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }
    long long effectiveMessages = total_messages + (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    // AI-driven optimization reduces core requirements
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 1500.0));
}

std::string TerahertzProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: Terahertz Communication with AI Beamforming" << std::endl;
    ss << "    Switching: Quantum Packet Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Base Band Channel: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Base Channel: " << users_per_channel << std::endl;
    ss << "    Terahertz Band: " << terahertz_band_mhz << " MHz at 300 GHz" << std::endl;
    ss << "    Users per THz MHz: " << users_per_terahertz_mhz << std::endl;
    ss << "    Ultra-Massive MIMO Antennas: " << NUM_ANTENNAS << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User: " << MESSAGES_PER_USER << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl << std::endl;
    ss << "  Advanced Features" << std::endl;
    ss << "    Quantum Encryption: " << (quantum_encryption ? std::string("Enabled") : std::string("Disabled")) << std::endl;
    ss << "    AI Beamforming: " << (ai_beamforming ? std::string("Enabled") : std::string("Disabled")) << std::endl;
    ss << "    Channel reuse across " << NUM_ANTENNAS << " antennas with AI optimization" << std::endl;
    return ss.str();
}

// HolographicProtocol implementation (7G)
HolographicProtocol::HolographicProtocol()
    : Protocol("Holographic (7G)", 3, 1.0, 100),
      satellite_band_mhz(500.0), users_per_satellite_mhz(100),
      holographic_beamforming(true), satellite_mesh(true), brain_interface(true) {}

HolographicProtocol::~HolographicProtocol() {}

int HolographicProtocol::calculate_max_users(double total_bandwidth_mhz) const
{
    // Base band calculations
    int baseBandChannels = calculate_total_channels(total_bandwidth_mhz);
    int baseBandUsers = baseBandChannels * users_per_channel;

    // Satellite mesh band users (500 MHz at visible light spectrum)
    int satelliteUsers = static_cast<int>(satellite_band_mhz) * users_per_satellite_mhz;

    // Extreme MIMO with 128 antennas + holographic beamforming multiplier
    return (baseBandUsers + satelliteUsers) * NUM_ANTENNAS * 2; // 2x from holographic
}

int HolographicProtocol::calculate_messages_per_user() const
{
    return MESSAGES_PER_USER;
}

int HolographicProtocol::calculate_required_cores(int totalUsers, int messages_per_user) const
{
    long long total_messages = static_cast<long long>(totalUsers) * static_cast<long long>(messages_per_user);
    if (totalUsers > 0 && messages_per_user > 0 && total_messages / messages_per_user != totalUsers) {
        throw CapacityException("Integer overflow while calculating total messages");
    }
    long long effectiveMessages = total_messages + (total_messages / 100) * static_cast<long long>(overhead_per_100_messages);
    // Brain-interface and quantum computing reduces core requirements significantly
    return static_cast<int>(std::ceil(static_cast<double>(effectiveMessages) / 2000.0));
}

std::string HolographicProtocol::get_protocol_details() const
{
    std::stringstream ss;
    ss << "Protocol: " << protocol_name << std::endl << std::endl;
    ss << "  Technical Specifications" << std::endl;
    ss << "    Access Method: Holographic Communication with Satellite Integration" << std::endl;
    ss << "    Switching: Neural Packet Switching" << std::endl << std::endl;
    ss << "  Channel Configuration" << std::endl;
    ss << "    Base Band Channel: " << channel_bandwidth << " kHz" << std::endl;
    ss << "    Users per Base Channel: " << users_per_channel << std::endl;
    ss << "    Satellite Mesh Band: " << satellite_band_mhz << " MHz (Visible Light Spectrum)" << std::endl;
    ss << "    Users per Satellite MHz: " << users_per_satellite_mhz << std::endl;
    ss << "    Extreme MIMO Antennas: " << NUM_ANTENNAS << std::endl << std::endl;
    ss << "  Performance Metrics" << std::endl;
    ss << "    Messages per User: " << MESSAGES_PER_USER << std::endl;
    ss << "    Overhead: " << overhead_per_100_messages << " per 100 messages" << std::endl << std::endl;
    ss << "  Advanced Features" << std::endl;
    ss << "    Holographic Beamforming: " << (holographic_beamforming ? std::string("Enabled") : std::string("Disabled")) << std::endl;
    ss << "    Satellite Mesh: " << (satellite_mesh ? std::string("Active") : std::string("Inactive")) << std::endl;
    ss << "    Brain Interface: " << (brain_interface ? std::string("Connected") : std::string("Disconnected")) << std::endl;
    ss << "    Channel reuse across " << NUM_ANTENNAS << " antennas with 2x holographic multiplier" << std::endl;
    return ss.str();
}
