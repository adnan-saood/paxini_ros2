#include "paxini_raw_hardware/tactile_raw_hardware_node.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace paxini_raw_hardware
{
namespace
{
std::string resultant_topic(const std::string & sensor_name)
{
  return "/paxini/" + sensor_name + "/resultant_force_raw";
}

std::string distributed_topic(const std::string & sensor_name)
{
  return "/paxini/" + sensor_name + "/taxel_forces_raw";
}
}  // namespace

TactileRawHardwareNode::TactileRawHardwareNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("paxini_raw_hardware_node", options)
{
  serial_port_ = declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
  baud_rate_ = static_cast<int>(declare_parameter<int64_t>("baud_rate", 921600));
  const auto publish_rate_hz = declare_parameter<double>("publish_rate_hz", 50.0);

  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    SensorPublishers sensor_publishers;
    sensor_publishers.resultant = create_publisher<geometry_msgs::msg::Vector3Stamped>(
      resultant_topic(sensor.name), rclcpp::SensorDataQoS());
    sensor_publishers.distributed = create_publisher<std_msgs::msg::Float32MultiArray>(
      distributed_topic(sensor.name), rclcpp::SensorDataQoS());
    publishers_.emplace(sensor.name, std::move(sensor_publishers));
  }

  const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz);
  timer_ = create_wall_timer(
    std::chrono::duration_cast<std::chrono::nanoseconds>(period),
    std::bind(&TactileRawHardwareNode::on_timer, this));

  calibration_service_ = create_service<std_srvs::srv::Trigger>(
    "/paxini/calibrate_sensors",
    std::bind(
      &TactileRawHardwareNode::on_calibrate_sensors, this, std::placeholders::_1,
      std::placeholders::_2));

  RCLCPP_INFO(
    get_logger(), "Publishing raw tactile data from '%s' at %.1f Hz on absolute topics under /paxini/*",
    serial_port_.c_str(), publish_rate_hz);
}

void TactileRawHardwareNode::on_timer()
{
  if (!bus_open_) {
    try {
      bus_.open(serial_port_, baud_rate_);
      bus_open_ = true;
      RCLCPP_INFO(get_logger(), "Opened tactile sensor serial port '%s'", serial_port_.c_str());
    } catch (const std::exception & error) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "Unable to open serial port '%s': %s",
        serial_port_.c_str(), error.what());
      return;
    }
  }

  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    try {
      const auto resultant = bus_.read_resultant(sensor);
      const auto distributed = bus_.read_distributed(sensor);

      const auto stamp = get_clock()->now();

      geometry_msgs::msg::Vector3Stamped resultant_message;
      resultant_message.header.stamp = stamp;
      resultant_message.header.frame_id = sensor.name + "_link";
      resultant_message.vector.x = resultant[0];
      resultant_message.vector.y = resultant[1];
      resultant_message.vector.z = resultant[2];

      std_msgs::msg::Float32MultiArray distributed_message;
      distributed_message.data = distributed;

      const auto & sensor_publishers = publishers_.at(sensor.name);
      sensor_publishers.resultant->publish(resultant_message);
      sensor_publishers.distributed->publish(distributed_message);
    } catch (const std::exception & error) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "Tactile sensor '%s' read failed: %s. Reopening port.",
        sensor.name.c_str(), error.what());
      bus_.close();
      bus_open_ = false;
      break;
    }
  }
}

void TactileRawHardwareNode::on_calibrate_sensors(
  const std::shared_ptr<std_srvs::srv::Trigger::Request>,
  std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  // Runs on the same (single-threaded) executor as on_timer(), so this never
  // races with the periodic read/publish cycle over the shared bus_.
  if (!bus_open_) {
    response->success = false;
    response->message = "Serial port '" + serial_port_ + "' is not open; cannot calibrate";
    return;
  }

  std::vector<std::string> failures;
  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    try {
      bus_.calibrate(sensor);
      RCLCPP_INFO(get_logger(), "Calibrated tactile sensor '%s'", sensor.name.c_str());
    } catch (const std::exception & error) {
      failures.emplace_back(sensor.name + ": " + error.what());
    }
  }

  if (failures.empty()) {
    response->success = true;
    response->message = "Calibrated " +
      std::to_string(paxini_hardware::supported_sensors().size()) + " sensor(s)";
    return;
  }

  response->success = false;
  std::string message = "Calibration failed for: ";
  for (std::size_t index = 0; index < failures.size(); ++index) {
    message += failures[index];
    if (index + 1 < failures.size()) {
      message += "; ";
    }
  }
  response->message = message;

  // A failed calibration exchange (e.g. a timeout mid-frame) can leave the
  // bus out of sync with the sensor's response framing; close it so the
  // next read cycle reopens and resynchronizes cleanly.
  bus_.close();
  bus_open_ = false;
}

}  // namespace paxini_raw_hardware
