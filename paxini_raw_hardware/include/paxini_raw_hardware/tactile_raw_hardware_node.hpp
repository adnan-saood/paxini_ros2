#ifndef PAXINI_RAW_HARDWARE__TACTILE_RAW_HARDWARE_NODE_HPP_
#define PAXINI_RAW_HARDWARE__TACTILE_RAW_HARDWARE_NODE_HPP_

#include <string>
#include <unordered_map>

#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "paxini_hardware/tactile_sensor.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"
#include "std_srvs/srv/trigger.hpp"

namespace paxini_raw_hardware
{

/// Sole owner of the Paxini tactile sensor serial bus.
///
/// Reads both the resultant and full per-taxel distributed force data for
/// every sensor in `paxini_hardware::supported_sensors()` and publishes them
/// on fixed, absolute topics:
///   /paxini/<sensor>/resultant_force_raw  (geometry_msgs/Vector3Stamped)
///   /paxini/<sensor>/taxel_forces_raw     (std_msgs/Float32MultiArray, flat x,y,z per taxel)
///
/// These topic names are absolute (leading '/') so they resolve identically
/// regardless of this node's or any subscriber's namespace/remapping,
/// avoiding the topic-name collisions that come from relative "~/" names.
///
/// This node does not use ros2_control: with hundreds of taxels per sensor,
/// exposing each one as an individual scalar hardware_interface state
/// interface does not scale (see paxini_hardware::PaxiniTactileHardware,
/// which instead bridges only the lightweight resultant force published
/// here into a handful of ros2_control state interfaces).
///
/// Also hosts a `/paxini/calibrate_sensors` (std_srvs/Trigger) service that
/// triggers each supported sensor's onboard hardware (zero-point)
/// calibration -- a firmware-level operation on the physical sensor, not a
/// software offset correction on the host.
class TactileRawHardwareNode : public rclcpp::Node
{
public:
  explicit TactileRawHardwareNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  struct SensorPublishers
  {
    rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr resultant;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr distributed;
  };

  void on_timer();
  void on_calibrate_sensors(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
    std::shared_ptr<std_srvs::srv::Trigger::Response> response);

  std::string serial_port_;
  int baud_rate_{921600};
  bool bus_open_{false};
  paxini_hardware::TactileSensorBus bus_;
  std::unordered_map<std::string, SensorPublishers> publishers_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr calibration_service_;
};

}  // namespace paxini_raw_hardware

#endif  // PAXINI_RAW_HARDWARE__TACTILE_RAW_HARDWARE_NODE_HPP_
