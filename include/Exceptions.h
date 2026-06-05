#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

// base exception class
class CellularNetworkException : public std::exception {
protected:
  std::string __message;

public:
  explicit CellularNetworkException(const std::string &msg) : __message(msg) {}
  virtual ~CellularNetworkException() noexcept {}

  virtual const char *what() const noexcept override {
    return __message.c_str();
  }
};

// capacity errors
class CapacityException : public CellularNetworkException {
public:
  explicit CapacityException(const std::string &msg)
      : CellularNetworkException("capacity error: " + msg) {}
};

// protocol errors
class ProtocolException : public CellularNetworkException {
public:
  explicit ProtocolException(const std::string &msg)
      : CellularNetworkException("protocol error: " + msg) {}
};

// config errors
class ConfigurationException : public CellularNetworkException {
public:
  explicit ConfigurationException(const std::string &msg)
      : CellularNetworkException("config error: " + msg) {}
};

// frequency errors
class FrequencyException : public CellularNetworkException {
public:
  explicit FrequencyException(const std::string &msg)
      : CellularNetworkException("frequency error: " + msg) {}
};

#endif // EXCEPTIONS_H
