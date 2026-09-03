#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "paxini_hardware/msg/tactile_array.hpp"
#include "paxini_hardware/tactile_sensor.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace paxini_controller
{

class TactileController : public rclcpp::Node
{
public:
  TactileController()
  : Node("paxini_tactile_controller"), serial_port_(declare_parameter<std::string>("serial_port", "/dev/ttyUSB0")),
    baud_rate_(declare_parameter<int64_t>("baud_rate", 921600)),
    poll_period_ms_(declare_parameter<int64_t>("poll_period_ms", 20))
  {
    if (baud_rate_ != 921600) {
      throw std::invalid_argument("Only the sensor protocol baud rate 921600 is supported");
    }
    if (poll_period_ms_ <= 0) {
      throw std::invalid_argument("Parameter 'poll_period_ms' must be positive");
    }

    for (const auto & sensor : paxini_hardware::supported_sensors()) {
      const auto topic = "paxini/" + sensor.name + "/tactile_data";
      publishers_.emplace(
        sensor.name,
        create_publisher<paxini_hardware::msg::TactileArray>(topic, rclcpp::SensorDataQoS()));
      RCLCPP_INFO(
        get_logger(), "Configured %s: module %u, address %u, %zu taxels, topic /%s",
        sensor.name.c_str(), sensor.module_id, sensor.device_address(), sensor.point_count(), topic.c_str());
    }

    poll_timer_ = create_wall_timer(
      std::chrono::milliseconds(poll_period_ms_), std::bind(&TactileController::poll_sensors, this));
  }

private:
  void connect_if_needed()
  {
    if (bus_.is_open()) {
      return;
    }
    try {
      bus_.open(serial_port_, static_cast<int>(baud_rate_));
      RCLCPP_INFO(
        get_logger(), "Connected to %s at %d baud", serial_port_.c_str(), static_cast<int>(baud_rate_));
    } catch (const std::exception & error) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000, "Cannot open %s: %s", serial_port_.c_str(), error.what());
    }
  }

  void poll_sensors()
  {
    connect_if_needed();
    if (!bus_.is_open()) {
      return;
    }

    for (const auto & sensor : paxini_hardware::supported_sensors()) {
      try {
        auto message = paxini_hardware::msg::TactileArray();
        message.header.stamp = now();
        message.header.frame_id = sensor.name + "_link";
        message.data = bus_.read_distributed(sensor);
        publishers_.at(sensor.name)->publish(message);
      } catch (const std::exception & error) {
        RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), 1000, "Failed to read %s: %s", sensor.name.c_str(), error.what());
        bus_.close();
        return;
      }
    }
  }

  std::string serial_port_;
  int64_t baud_rate_;
  int64_t poll_period_ms_;
  paxini_hardware::TactileSensorBus bus_;
  std::unordered_map<std::string, rclcpp::Publisher<paxini_hardware::msg::TactileArray>::SharedPtr> publishers_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
};

}  // namespace paxini_controller

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<paxini_controller::TactileController>());
  rclcpp::shutdown();
  return 0;
}
