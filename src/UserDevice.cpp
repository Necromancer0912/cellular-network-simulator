#include "UserDevice.h"
#include "Utils.h"
#include "IOHelpers.h"

// Initialize static member
int UserDevice::device_counter = 0;

// UserDevice implementation
UserDevice::UserDevice(const std::string &name, CommunicationType type)
    : device_name(name), comm_type(type), is_connected(false),
      assigned_channel(-1), assigned_antenna(-1), x(0.0), y(0.0)
{
    device_id = ++device_counter;
}

UserDevice::UserDevice(const UserDevice &other)
    : device_name(other.device_name), comm_type(other.comm_type),
      is_connected(other.is_connected), assigned_channel(other.assigned_channel),
      assigned_antenna(other.assigned_antenna), x(other.x), y(other.y)
{
    device_id = ++device_counter;
}

UserDevice::~UserDevice()
{
    // Destructor
}

UserDevice &UserDevice::operator=(const UserDevice &other)
{
    if (this != &other)
    {
        device_name = other.device_name;
        comm_type = other.comm_type;
        is_connected = other.is_connected;
        assigned_channel = other.assigned_channel;
        assigned_antenna = other.assigned_antenna;
        x = other.x;
        y = other.y;
    }
    return *this;
}

std::string UserDevice::get_communication_type_string() const
{
    switch (comm_type)
    {
    case CommunicationType::DATA:
        return "Data";
    case CommunicationType::VOICE:
        return "Voice";
    case CommunicationType::BOTH:
        return "Data & Voice";
    default:
        return "Unknown";
    }
}

void UserDevice::display_info() const
{
    IOHelpers::print("    Device ID: ");
    IOHelpers::printInt(device_id);
    IOHelpers::printNewline();
    
    IOHelpers::print("    Name: ");
    IOHelpers::println(device_name.c_str());
    
    IOHelpers::print("    Type: ");
    IOHelpers::println(get_device_type().c_str());
    
    IOHelpers::print("    Communication: ");
    IOHelpers::println(get_communication_type_string().c_str());
    
    IOHelpers::print("    Messages: ");
    IOHelpers::printInt(get_message_count());
    IOHelpers::printNewline();

    IOHelpers::print("    Status: ");
    IOHelpers::println(is_connected ? "Connected" : "Disconnected");

    if (is_connected)
    {
        IOHelpers::print("    Channel: ");
        IOHelpers::printInt(assigned_channel);
        IOHelpers::printNewline();
        
        if (assigned_antenna >= 0)
        {
            IOHelpers::print("    Antenna: ");
            IOHelpers::printInt(assigned_antenna);
            IOHelpers::printNewline();
        }
    }
}

// Device2G implementation
Device2G::Device2G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device2G::~Device2G() {}

int Device2G::get_message_count() const
{
    switch (get_communication_type())
    {
    case CommunicationType::DATA:
        return 5;
    case CommunicationType::VOICE:
        return 15;
    case CommunicationType::BOTH:
        return 20;
    default:
        return 0;
    }
}

// Device3G implementation
Device3G::Device3G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device3G::~Device3G() {}

int Device3G::get_message_count() const
{
    return 10; // Always 10 messages for 3G
}

// Device4G implementation
Device4G::Device4G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device4G::~Device4G() {}

int Device4G::get_message_count() const
{
    return 10; // Always 10 messages for 4G
}

// Device5G implementation
Device5G::Device5G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device5G::~Device5G() {}

int Device5G::get_message_count() const
{
    return 10; // Always 10 messages for 5G
}

// Device6G implementation
Device6G::Device6G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device6G::~Device6G() {}

int Device6G::get_message_count() const
{
    return 8; // 8 messages for 6G
}

// Device7G implementation
Device7G::Device7G(const std::string &name, CommunicationType type)
    : UserDevice(name, type) {}

Device7G::~Device7G() {}

int Device7G::get_message_count() const
{
    return 6; // 6 messages for 7G
}
