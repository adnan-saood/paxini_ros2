#ifndef PAXINI_RAW_CONTROLLER__TACTILE_RAW_CONTROLLER_NODE_HPP_
#define PAXINI_RAW_CONTROLLER__TACTILE_RAW_CONTROLLER_NODE_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "paxini_hardware/msg/tactile_sensor.hpp"
#include "paxini_hardware/taxel_geometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"
#include "visualization_msgs/msg/marker_array.hpp"

namespace paxini_raw_controller
{

/// Subscribes to the raw per-sensor topics published by paxini_raw_hardware
/// (`/paxini/<sensor>/resultant_force_raw` and `/paxini/<sensor>/taxel_forces_raw`),
/// enriches the per-taxel force data with the static taxel geometry loaded
/// from paxini_hardware's calibration CSVs, and publishes:
///   /paxini/<sensor>/tactile_sensor  (paxini_hardware/TactileSensor)
///   /paxini/<sensor>/taxel_markers   (visualization_msgs/MarkerArray, for RViz)
///
/// This runs entirely as a plain rclcpp node, outside of ros2_control/
/// controller_manager, since this is the full-resolution (hundreds of
/// taxels) data path that does not fit ros2_control's scalar
/// state-interface model.
class TactileRawControllerNode : public rclcpp::Node
{
public:
  explicit TactileRawControllerNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  struct SensorState
  {
    rclcpp::Subscription<geometry_msgs::msg::Vector3Stamped>::SharedPtr resultant_subscription;
    rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr distributed_subscription;
    rclcpp::Publisher<paxini_hardware::msg::TactileSensor>::SharedPtr sensor_publisher;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_publisher;
    geometry_msgs::msg::Vector3Stamped last_resultant;
    bool has_resultant{false};
  };

  void on_resultant(const std::string & sensor_name, const geometry_msgs::msg::Vector3Stamped::SharedPtr message);
  void on_distributed(const std::string & sensor_name, const std_msgs::msg::Float32MultiArray::SharedPtr message);
  void publish_markers(const std::string & sensor_name, const paxini_hardware::msg::TactileSensor & message);

  std::unordered_map<std::string, std::vector<paxini_hardware::TaxelGeometry>> geometry_;
  std::unordered_map<std::string, SensorState> sensors_;
};

}  // namespace paxini_raw_controller

#endif  // PAXINI_RAW_CONTROLLER__TACTILE_RAW_CONTROLLER_NODE_HPP_
