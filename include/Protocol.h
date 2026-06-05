#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <memory>

/**
 * Abstract base class for communication protocols
 * Demonstrates polymorphism and data abstraction
 */
class Protocol
{
protected:
    std::string protocol_name;
    int overhead_per_100_messages;
    double channel_bandwidth; // in kHz
    int users_per_channel;

public:
    Protocol(const std::string &name, int overhead, double bandwidth, int users);
    virtual ~Protocol();

    // Pure virtual methods
    virtual int calculate_max_users(double total_bandwidth_mhz) const = 0;
    virtual int calculate_messages_per_user() const = 0;
    virtual int calculate_required_cores(int totalUsers, int messages_per_user) const = 0;
    virtual std::string get_protocol_details() const = 0;

    // Getters
    std::string get_protocol_name() const { return protocol_name; }
    int get_overhead() const { return overhead_per_100_messages; }
    double get_channel_bandwidth() const { return channel_bandwidth; }
    int get_users_per_channel() const { return users_per_channel; }

    // Common utility
    int calculate_total_channels(double total_bandwidth_mhz) const;
};

/**
 * 2G Protocol - TDMA with circuit and packet switching
 */
class TDMAProtocol : public Protocol
{
private:
    static const int DATA_MESSAGES = 5;
    static const int VOICE_MESSAGES = 15;

public:
    TDMAProtocol();
    virtual ~TDMAProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;

    static int get_data_messages() { return DATA_MESSAGES; }
    static int get_voice_messages() { return VOICE_MESSAGES; }
};

/**
 * 3G Protocol - CDMA with packet switching
 */
class CDMAProtocol : public Protocol
{
private:
    static const int MESSAGES_PER_USER = 10;

public:
    CDMAProtocol();
    virtual ~CDMAProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;
};

/**
 * 4G Protocol - OFDM with MIMO
 */
class OFDMProtocol : public Protocol
{
private:
    static const int MESSAGES_PER_USER = 10;
    static const int NUM_ANTENNAS = 4;

public:
    OFDMProtocol();
    virtual ~OFDMProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;

    static int get_num_antennas() { return NUM_ANTENNAS; }
};

/**
 * 5G Protocol - Massive MIMO with higher frequency
 */
class MassiveMIMOProtocol : public Protocol
{
private:
    static const int MESSAGES_PER_USER = 10;
    static const int NUM_ANTENNAS = 16;
    double high_frequency_band_mhz; // Additional 10 MHz at 1800 MHz
    int users_per_high_freq_mhz;

public:
    MassiveMIMOProtocol();
    virtual ~MassiveMIMOProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;

    static int get_num_antennas() { return NUM_ANTENNAS; }
    double get_high_frequency_band() const { return high_frequency_band_mhz; }
    int get_users_per_high_freq_mhz() const { return users_per_high_freq_mhz; }
};

/**
 * 6G Protocol - Terahertz communication with AI-driven beamforming
 * Ultra-massive MIMO with 64 antennas, quantum encryption support
 */
class TerahertzProtocol : public Protocol
{
private:
    static const int MESSAGES_PER_USER = 8;
    static const int NUM_ANTENNAS = 64;
    double terahertz_band_mhz; // 100 MHz at 300 GHz
    int users_per_terahertz_mhz;
    bool quantum_encryption;
    bool ai_beamforming;

public:
    TerahertzProtocol();
    virtual ~TerahertzProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;

    static int get_num_antennas() { return NUM_ANTENNAS; }
    double get_terahertz_band() const { return terahertz_band_mhz; }
    bool has_quantum_encryption() const { return quantum_encryption; }
    bool has_ai_beamforming() const { return ai_beamforming; }
};

/**
 * 7G Protocol - Holographic communication with satellite integration
 * Extreme MIMO with 128 antennas, holographic beamforming, satellite mesh
 */
class HolographicProtocol : public Protocol
{
private:
    static const int MESSAGES_PER_USER = 6;
    static const int NUM_ANTENNAS = 128;
    double satellite_band_mhz; // 500 MHz at visible light spectrum
    int users_per_satellite_mhz;
    bool holographic_beamforming;
    bool satellite_mesh;
    bool brain_interface;

public:
    HolographicProtocol();
    virtual ~HolographicProtocol();

    int calculate_max_users(double total_bandwidth_mhz) const override;
    int calculate_messages_per_user() const override;
    int calculate_required_cores(int totalUsers, int messages_per_user) const override;
    std::string get_protocol_details() const override;

    static int get_num_antennas() { return NUM_ANTENNAS; }
    double get_satellite_band() const { return satellite_band_mhz; }
    bool has_holographic_beamforming() const { return holographic_beamforming; }
    bool has_satellite_mesh() const { return satellite_mesh; }
    bool has_brain_interface() const { return brain_interface; }
};

#endif // PROTOCOL_H
