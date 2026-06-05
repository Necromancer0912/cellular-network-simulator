#ifndef USERDEVICE_H
#define USERDEVICE_H

#include <string>
#include <memory>

/**
 * Enumeration for device communication types
 */
enum class CommunicationType
{
    DATA,
    VOICE,
    BOTH
};

/**
 * Base class representing a user device in the cellular network
 * Demonstrates data abstraction and encapsulation
 */
class UserDevice
{
private:
    static int device_counter;
    int device_id;
    std::string device_name;
    CommunicationType comm_type;
    bool is_connected;
    int assigned_channel;
    int assigned_antenna;
    double x;
    double y;

public:
    // Constructors
    UserDevice(const std::string &name, CommunicationType type);
    UserDevice(const UserDevice &other);
    virtual ~UserDevice();

    // Assignment operator
    UserDevice &operator=(const UserDevice &other);

    // Getters
    int get_device_id() const { return device_id; }
    std::string get_device_name() const { return device_name; }
    CommunicationType get_communication_type() const { return comm_type; }
    bool get_connection_status() const { return is_connected; }
    int get_assigned_channel() const { return assigned_channel; }
    int get_assigned_antenna() const { return assigned_antenna; }
    double get_x() const { return x; }
    double get_y() const { return y; }

    // Setters
    void set_connection_status(bool status) { is_connected = status; }
    void set_assigned_channel(int channel) { assigned_channel = channel; }
    void set_assigned_antenna(int antenna) { assigned_antenna = antenna; }
    void set_position(double new_x, double new_y) { x = new_x; y = new_y; }

    // Virtual methods for polymorphism
    virtual int get_message_count() const = 0;
    virtual std::string get_device_type() const = 0;

    // Utility methods
    std::string get_communication_type_string() const;
    void display_info() const;

    // Static method
    static int get_total_devices() { return device_counter; }
};

/**
 * 2G Device - uses circuit switching for voice
 */
class Device2G : public UserDevice
{
public:
    Device2G(const std::string &name, CommunicationType type);
    virtual ~Device2G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "2G"; }
};

/**
 * 3G Device - uses packet switching for both
 */
class Device3G : public UserDevice
{
public:
    Device3G(const std::string &name, CommunicationType type);
    virtual ~Device3G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "3G"; }
};

/**
 * 4G Device - uses OFDM
 */
class Device4G : public UserDevice
{
public:
    Device4G(const std::string &name, CommunicationType type);
    virtual ~Device4G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "4G"; }
};

/**
 * 5G Device - uses massive MIMO
 */
class Device5G : public UserDevice
{
public:
    Device5G(const std::string &name, CommunicationType type);
    virtual ~Device5G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "5G"; }
};

/**
 * 6G Device - uses terahertz communication with quantum encryption
 */
class Device6G : public UserDevice
{
public:
    Device6G(const std::string &name, CommunicationType type);
    virtual ~Device6G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "6G"; }
};

/**
 * 7G Device - uses holographic communication with brain interface
 */
class Device7G : public UserDevice
{
public:
    Device7G(const std::string &name, CommunicationType type);
    virtual ~Device7G();

    int get_message_count() const override;
    std::string get_device_type() const override { return "7G"; }
};

#endif // USERDEVICE_H
