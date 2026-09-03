#include "paxini_controller/tactile_broadcaster.hpp"

#include <stdexcept>
#include <string>

#include "pluginlib/class_list_macros.hpp"
#include "paxini_hardware/tactile_sensor.hpp"

namespace paxini_controller
{

controller_interface::CallbackReturn TactileBroadcaster::on_init()
{
  return controller_interface::CallbackReturn::SUCCESS;
}

controller_interface::InterfaceConfiguration TactileBroadcaster::command_interface_configuration() const
{
  return {controller_interface::interface_configuration_type::NONE};
}

controller_interface::InterfaceConfiguration TactileBroadcaster::state_interface_configuration() const
{
  controller_interface::InterfaceConfiguration configuration;
  configuration.type = controller_interface::interface_configuration_type::INDIVIDUAL;
  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    const auto prefix = sensor.name + "/";
    configuration.names.push_back(prefix + "resultant_force_x");
    configuration.names.push_back(prefix + "resultant_force_y");
    configuration.names.push_back(prefix + "resultant_force_z");
  }
  return configuration;
}

controller_interface::CallbackReturn TactileBroadcaster::on_configure(const rclcpp_lifecycle::State &)
{
  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    publishers_.emplace(
      sensor.name,
      get_node()->create_publisher<geometry_msgs::msg::Vector3Stamped>(
        "~/" + sensor.name + "/resultant_force", rclcpp::SensorDataQoS()));
  }
  return controller_interface::CallbackReturn::SUCCESS;
}

controller_interface::CallbackReturn TactileBroadcaster::on_activate(const rclcpp_lifecycle::State &)
{
  // Build a name -> index cache once here instead of linearly scanning all
  // claimed state interfaces on every state_value() call in update().
  state_index_.clear();
  state_index_.reserve(state_interfaces_.size());
  for (std::size_t index = 0; index < state_interfaces_.size(); ++index) {
    state_index_.emplace(state_interfaces_[index].get_name(), index);
  }
  return controller_interface::CallbackReturn::SUCCESS;
}

double TactileBroadcaster::state_value(const std::string & sensor, const std::string & interface_name) const
{
  // hardware_interface::ReadOnlyHandle::get_name() already returns the full
  // "prefix/interface" name, so it must be compared directly (not concatenated
  // again with get_interface_name()) or this lookup will never match.
  const auto full_name = sensor + "/" + interface_name;
  const auto found = state_index_.find(full_name);
  if (found == state_index_.end()) {
    throw std::runtime_error("Missing state interface: " + full_name);
  }
  return state_interfaces_[found->second].get_value();
}

controller_interface::return_type TactileBroadcaster::update(const rclcpp::Time & time, const rclcpp::Duration &)
{
  try {
    for (const auto & sensor : paxini_hardware::supported_sensors()) {
      geometry_msgs::msg::Vector3Stamped message;
      message.header.stamp = time;
      message.header.frame_id = sensor.name + "_link";
      message.vector.x = state_value(sensor.name, "resultant_force_x");
      message.vector.y = state_value(sensor.name, "resultant_force_y");
      message.vector.z = state_value(sensor.name, "resultant_force_z");
      publishers_.at(sensor.name)->publish(message);
    }
  } catch (const std::exception &) {return controller_interface::return_type::ERROR;}
  return controller_interface::return_type::OK;
}
}  // namespace paxini_controller

PLUGINLIB_EXPORT_CLASS(paxini_controller::TactileBroadcaster, controller_interface::ControllerInterface)
