#include "paxini_hardware/tactile_sensor.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace paxini_hardware
{
namespace
{
constexpr std::size_t kResponsePrefixLength = 14;
constexpr std::array<uint8_t, 2> kResponseHeader{0xAA, 0x55};

speed_t baud_rate_to_termios(int baud_rate)
{
  switch (baud_rate) {
    case 921600:
      return B921600;
    default:
      throw std::invalid_argument("Unsupported baud rate: " + std::to_string(baud_rate));
  }
}

uint8_t calculate_lrc(const std::vector<uint8_t> & bytes)
{
  uint32_t sum = 0;
  for (const auto byte : bytes) {
    sum += byte;
  }
  return static_cast<uint8_t>((-static_cast<int32_t>(sum)) & 0xFF);
}

std::vector<uint8_t> distributed_command(const SensorConfig & sensor)
{
  const auto length_low = static_cast<uint8_t>(sensor.distributed_length & 0xFFU);
  const auto length_high = static_cast<uint8_t>(sensor.distributed_length >> 8U);
  std::vector<uint8_t> command{
    0x55, 0xAA, 0x09, 0x00, sensor.device_address(), 0x00, 0xFB,
    0x0E, 0x04, 0x00, 0x00, length_low, length_high};
  command.push_back(calculate_lrc(command));
  return command;
}

std::string system_error(const std::string & operation)
{
  return operation + ": " + std::strerror(errno);
}
}  // namespace

uint8_t SensorConfig::device_address() const
{
  return static_cast<uint8_t>(module_id + 1U);
}

std::size_t SensorConfig::point_count() const
{
  return distributed_length / 3U;
}

const std::vector<SensorConfig> & supported_sensors()
{
  static const std::vector<SensorConfig> sensors{
    {"L5325_omega", 0, 717},
    {"S1813_elite", 2, 93},
  };
  return sensors;
}

TactileSensorBus::TactileSensorBus()
: file_descriptor_(-1)
{
}

TactileSensorBus::~TactileSensorBus()
{
  close();
}

void TactileSensorBus::open(const std::string & device, int baud_rate)
{
  close();
  file_descriptor_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (file_descriptor_ < 0) {
    throw std::runtime_error(system_error("Unable to open serial device " + device));
  }

  termios settings{};
  if (tcgetattr(file_descriptor_, &settings) != 0) {
    const auto error = system_error("Unable to read serial settings");
    close();
    throw std::runtime_error(error);
  }

  cfmakeraw(&settings);
  const auto speed = baud_rate_to_termios(baud_rate);
  if (cfsetispeed(&settings, speed) != 0 || cfsetospeed(&settings, speed) != 0) {
    const auto error = system_error("Unable to set serial baud rate");
    close();
    throw std::runtime_error(error);
  }
  settings.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
  settings.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
  settings.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
  settings.c_cc[VMIN] = 0;
  settings.c_cc[VTIME] = 0;
  if (tcsetattr(file_descriptor_, TCSANOW, &settings) != 0) {
    const auto error = system_error("Unable to configure serial device");
    close();
    throw std::runtime_error(error);
  }
  tcflush(file_descriptor_, TCIOFLUSH);
}

void TactileSensorBus::close()
{
  if (file_descriptor_ >= 0) {
    ::close(file_descriptor_);
    file_descriptor_ = -1;
  }
}

bool TactileSensorBus::is_open() const
{
  return file_descriptor_ >= 0;
}

void TactileSensorBus::write_all(const std::vector<uint8_t> & command) const
{
  std::size_t sent = 0;
  while (sent < command.size()) {
    const auto written = ::write(file_descriptor_, command.data() + sent, command.size() - sent);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(system_error("Unable to write sensor request"));
    }
    sent += static_cast<std::size_t>(written);
  }
}

std::vector<uint8_t> TactileSensorBus::read_response(std::size_t payload_length, int timeout_ms) const
{
  const auto expected_size = kResponsePrefixLength + payload_length;
  std::vector<uint8_t> response;
  response.reserve(expected_size);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

  while (std::chrono::steady_clock::now() < deadline) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - std::chrono::steady_clock::now()).count();
    pollfd descriptor{file_descriptor_, POLLIN, 0};
    const auto ready = poll(&descriptor, 1, static_cast<int>(std::max<int64_t>(1, remaining)));
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(system_error("Unable to read sensor response"));
    }
    if (ready == 0) {
      continue;
    }

    std::array<uint8_t, 256> buffer{};
    const auto count = ::read(file_descriptor_, buffer.data(), buffer.size());
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error(system_error("Unable to read sensor response"));
    }
    response.insert(response.end(), buffer.begin(), buffer.begin() + count);

    const auto header = std::search(response.begin(), response.end(), kResponseHeader.begin(), kResponseHeader.end());
    if (header != response.end()) {
      const auto available = static_cast<std::size_t>(response.end() - header);
      if (available >= expected_size) {
        return std::vector<uint8_t>(header + kResponsePrefixLength, header + expected_size);
      }
    } else if (response.size() > 1) {
      response.erase(response.begin(), response.end() - 1);
    }
  }
  throw std::runtime_error("Timed out waiting for tactile sensor response");
}

std::vector<float> TactileSensorBus::read_distributed(const SensorConfig & sensor, int timeout_ms)
{
  if (!is_open()) {
    throw std::runtime_error("Serial device is not open");
  }

  tcflush(file_descriptor_, TCIFLUSH);
  write_all(distributed_command(sensor));
  const auto payload = read_response(sensor.distributed_length, timeout_ms);

  std::vector<float> values;
  values.reserve(payload.size());
  for (std::size_t index = 0; index + 2 < payload.size(); index += 3) {
    const auto x = static_cast<int8_t>(payload[index]);
    const auto y = static_cast<int8_t>(payload[index + 1]);
    const auto z = payload[index + 2];
    values.push_back(static_cast<float>(x) * 0.1F);
    values.push_back(static_cast<float>(y) * 0.1F);
    values.push_back(static_cast<float>(z) * 0.1F);
  }
  return values;
}

}  // namespace paxini_hardware
