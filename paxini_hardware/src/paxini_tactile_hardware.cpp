#include "paxini_hardware/paxini_tactile_hardware.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "pluginlib/class_list_macros.hpp"

namespace paxini_hardware
{
namespace
{
std::string interface_key(const std::string & sensor_name, const std::string & interface_name)
{
  return sensor_name + "/" + interface_name;
}

// Fixed, absolute topic name shared with paxini_raw_hardware. Using an
// absolute (leading '/') name means the connection does not depend on either
// node's namespace/remapping, avoiding the topic-name collisions that arise
// from relative "~/" names when multiple nodes/controllers are involved.
std::string resultant_topic(const std::string & sensor_name)
{
  return "/paxini/" + sensor_name + "/resultant_force_raw";
}
}  // namespace

hardware_interface::CallbackReturn PaxiniTactileHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SensorInterface::on_init(info) != hardware_interface::CallbackReturn::SUCCESS) {
    return hardware_interface::CallbackReturn::ERROR;
  }

  try {
    for (const auto & sensor : info_.sensors) {
      cached_forces_.emplace(sensor.name, std::make_unique<CachedForce>());
      for (const auto & interface : sensor.state_interfaces) {
        const auto key = interface_key(sensor.name, interface.name);
        if (!state_indices_.emplace(key, state_values_.size()).second) {
          throw std::invalid_argument("Duplicate state interface: " + key);
        }
        state_values_.push_back(0.0);
      }
    }
  } catch (const std::exception &) {
    return hardware_interface::CallbackReturn::ERROR;
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PaxiniTactileHardware::on_configure(
  const rclcpp_lifecycle::State &)
{
  // This component does not open the serial port itself. It only subscribes
  // to the resultant-force topic published by paxini_raw_hardware, which is
  // the sole owner of the physical bus. See the class documentation in
  // paxini_tactile_hardware.hpp for the rationale.
  try {
    node_ = std::make_shared<rclcpp::Node>("paxini_tactile_hardware_bridge");
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(node_);

    subscriptions_.clear();
    for (const auto & sensor : info_.sensors) {
      auto * const cache = cached_forces_.at(sensor.name).get();
      auto subscription = node_->create_subscription<geometry_msgs::msg::Vector3Stamped>(
        resultant_topic(sensor.name), rclcpp::SensorDataQoS(),
        [cache](const geometry_msgs::msg::Vector3Stamped::SharedPtr message) {
          cache->x.store(message->vector.x, std::memory_order_relaxed);
          cache->y.store(message->vector.y, std::memory_order_relaxed);
          cache->z.store(message->vector.z, std::memory_order_relaxed);
        });
      subscriptions_.push_back(std::move(subscription));
    }
  } catch (const std::exception &) {
    return hardware_interface::CallbackReturn::ERROR;
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PaxiniTactileHardware::on_activate(
  const rclcpp_lifecycle::State &)
{
  if (!executor_) {
    return hardware_interface::CallbackReturn::ERROR;
  }
  spin_thread_ = std::thread([this]() {executor_->spin();});
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn PaxiniTactileHardware::on_deactivate(
  const rclcpp_lifecycle::State &)
{
  stop_executor();
  return hardware_interface::CallbackReturn::SUCCESS;
}

void PaxiniTactileHardware::stop_executor()
{
  if (executor_) {
    executor_->cancel();
  }
  if (spin_thread_.joinable()) {
    spin_thread_.join();
  }
}

std::vector<hardware_interface::StateInterface> PaxiniTactileHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> interfaces;
  interfaces.reserve(state_values_.size());
  for (const auto & sensor : info_.sensors) {
    for (const auto & interface : sensor.state_interfaces) {
      const auto index = state_indices_.at(interface_key(sensor.name, interface.name));
      interfaces.emplace_back(sensor.name, interface.name, &state_values_[index]);
    }
  }
  return interfaces;
}

hardware_interface::return_type PaxiniTactileHardware::read(
  const rclcpp::Time &, const rclcpp::Duration &)
{
  for (const auto & sensor : info_.sensors) {
    const auto & cache = *cached_forces_.at(sensor.name);
    state_values_.at(state_indices_.at(interface_key(sensor.name, "resultant_force_x"))) =
      cache.x.load(std::memory_order_relaxed);
    state_values_.at(state_indices_.at(interface_key(sensor.name, "resultant_force_y"))) =
      cache.y.load(std::memory_order_relaxed);
    state_values_.at(state_indices_.at(interface_key(sensor.name, "resultant_force_z"))) =
      cache.z.load(std::memory_order_relaxed);
  }
  return hardware_interface::return_type::OK;
}

}  // namespace paxini_hardware

PLUGINLIB_EXPORT_CLASS(paxini_hardware::PaxiniTactileHardware, hardware_interface::SensorInterface)
