#ifndef PAXINI_CONTROLLER__TACTILE_BROADCASTER_HPP_
#define PAXINI_CONTROLLER__TACTILE_BROADCASTER_HPP_

#include <string>
#include <unordered_map>

#include "controller_interface/controller_interface.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace paxini_controller
{

/// Minimal example controller demonstrating how to claim the resultant
/// (summary) force state interfaces exposed by paxini_hardware/
/// PaxiniTactileHardware and republish them as regular topics.
///
/// This controller intentionally does NOT handle per-taxel data or
/// visualization markers: with hundreds of taxels per sensor, routing that
/// volume of data through ros2_control's scalar state-interface mechanism
/// does not scale (see paxini_hardware/PaxiniTactileHardware's class
/// documentation). That full-resolution pipeline is handled entirely outside
/// of ros2_control by the paxini_raw_hardware/paxini_raw_controller nodes.
class TactileBroadcaster : public controller_interface::ControllerInterface
{
public:
  controller_interface::CallbackReturn on_init() override;
  controller_interface::InterfaceConfiguration command_interface_configuration() const override;
  controller_interface::InterfaceConfiguration state_interface_configuration() const override;
  controller_interface::CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::return_type update(const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  std::unordered_map<std::string, rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr>
    publishers_;
  // Maps "sensor/interface" -> index into state_interfaces_, built once in
  // on_activate() so update() does O(1) lookups instead of a linear scan.
  std::unordered_map<std::string, std::size_t> state_index_;

  double state_value(const std::string & sensor, const std::string & interface_name) const;
};

}  // namespace paxini_controller

#endif  // PAXINI_CONTROLLER__TACTILE_BROADCASTER_HPP_
