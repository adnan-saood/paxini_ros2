#include "paxini_raw_controller/tactile_raw_controller_node.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "paxini_hardware/tactile_sensor.hpp"
#include "std_msgs/msg/color_rgba.hpp"

namespace paxini_raw_controller
{
namespace
{
constexpr double kForceArrowScale = 0.01;

std::string resultant_topic(const std::string & sensor_name)
{
  return "/paxini/" + sensor_name + "/resultant_force_raw";
}

std::string distributed_topic(const std::string & sensor_name)
{
  return "/paxini/" + sensor_name + "/taxel_forces_raw";
}

std_msgs::msg::ColorRGBA color_for_force(const double magnitude)
{
  const auto level = std::min(1.0, magnitude / 10.0);
  std_msgs::msg::ColorRGBA color;
  color.r = static_cast<float>(level);
  color.g = static_cast<float>(1.0 - level);
  color.a = 1.0F;
  return color;
}
}  // namespace

TactileRawControllerNode::TactileRawControllerNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("paxini_raw_controller_node", options)
{
  // Throws std::runtime_error if a geometry CSV is missing/malformed; this is
  // treated as a fatal startup error, matching the previous controller's
  // on_init() behaviour, and is caught/logged in main().
  geometry_ = paxini_hardware::load_all_taxel_geometry();

  for (const auto & sensor : paxini_hardware::supported_sensors()) {
    SensorState state;
    state.sensor_publisher = create_publisher<paxini_hardware::msg::TactileSensor>(
      "/paxini/" + sensor.name + "/tactile_sensor", rclcpp::SensorDataQoS());
    state.marker_publisher = create_publisher<visualization_msgs::msg::MarkerArray>(
      "/paxini/" + sensor.name + "/taxel_markers", rclcpp::QoS(1).transient_local());

    const auto sensor_name = sensor.name;
    state.resultant_subscription = create_subscription<geometry_msgs::msg::Vector3Stamped>(
      resultant_topic(sensor_name), rclcpp::SensorDataQoS(),
      [this, sensor_name](const geometry_msgs::msg::Vector3Stamped::SharedPtr message) {
        on_resultant(sensor_name, message);
      });
    state.distributed_subscription = create_subscription<std_msgs::msg::Float32MultiArray>(
      distributed_topic(sensor_name), rclcpp::SensorDataQoS(),
      [this, sensor_name](const std_msgs::msg::Float32MultiArray::SharedPtr message) {
        on_distributed(sensor_name, message);
      });

    sensors_.emplace(sensor_name, std::move(state));
  }

  RCLCPP_INFO(
    get_logger(),
    "Publishing enriched tactile data + RViz markers on absolute topics under /paxini/*");
}

void TactileRawControllerNode::on_resultant(
  const std::string & sensor_name, const geometry_msgs::msg::Vector3Stamped::SharedPtr message)
{
  auto & state = sensors_.at(sensor_name);
  state.last_resultant = *message;
  state.has_resultant = true;
}

void TactileRawControllerNode::on_distributed(
  const std::string & sensor_name, const std_msgs::msg::Float32MultiArray::SharedPtr message)
{
  auto & state = sensors_.at(sensor_name);
  const auto & taxels = geometry_.at(sensor_name);

  if (message->data.size() != taxels.size() * 3U) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "Sensor '%s': expected %zu distributed values, got %zu. Dropping message.",
      sensor_name.c_str(), taxels.size() * 3U, message->data.size());
    return;
  }

  paxini_hardware::msg::TactileSensor sensor_message;
  if (state.has_resultant) {
    sensor_message.header.stamp = state.last_resultant.header.stamp;
  } else {
    sensor_message.header.stamp = get_clock()->now();
  }
  sensor_message.header.frame_id = sensor_name + "_link";
  if (state.has_resultant) {
    sensor_message.resultant_force = state.last_resultant.vector;
  }

  sensor_message.taxels.reserve(taxels.size());
  for (const auto & geometry : taxels) {
    paxini_hardware::msg::TactileTaxel taxel;
    taxel.index = geometry.index;
    taxel.position = geometry.position;
    taxel.force.x = message->data[geometry.index * 3U];
    taxel.force.y = message->data[geometry.index * 3U + 1U];
    taxel.force.z = message->data[geometry.index * 3U + 2U];
    sensor_message.taxels.push_back(std::move(taxel));
  }

  state.sensor_publisher->publish(sensor_message);
  publish_markers(sensor_name, sensor_message);
}

void TactileRawControllerNode::publish_markers(
  const std::string & sensor_name, const paxini_hardware::msg::TactileSensor & message)
{
  visualization_msgs::msg::MarkerArray markers;
  visualization_msgs::msg::Marker clear;
  clear.header = message.header;
  clear.action = visualization_msgs::msg::Marker::DELETEALL;
  markers.markers.push_back(clear);

  for (const auto & taxel : message.taxels) {
    const auto magnitude = std::sqrt(
      taxel.force.x * taxel.force.x + taxel.force.y * taxel.force.y + taxel.force.z * taxel.force.z);

    visualization_msgs::msg::Marker sphere;
    sphere.header = message.header;
    sphere.ns = "taxels";
    sphere.id = static_cast<int32_t>(taxel.index);
    sphere.type = visualization_msgs::msg::Marker::SPHERE;
    sphere.action = visualization_msgs::msg::Marker::ADD;
    sphere.pose.position = taxel.position;
    sphere.pose.orientation.w = 1.0;
    sphere.scale.x = 0.002;
    sphere.scale.y = 0.002;
    sphere.scale.z = 0.002;
    sphere.color = color_for_force(magnitude);
    markers.markers.push_back(std::move(sphere));

    if (magnitude > 0.0) {
      visualization_msgs::msg::Marker arrow;
      arrow.header = message.header;
      arrow.ns = "forces";
      arrow.id = static_cast<int32_t>(taxel.index);
      arrow.type = visualization_msgs::msg::Marker::ARROW;
      arrow.action = visualization_msgs::msg::Marker::ADD;
      arrow.scale.x = 0.0015;
      arrow.scale.y = 0.003;
      arrow.color = color_for_force(magnitude);
      arrow.points.push_back(taxel.position);
      auto endpoint = taxel.position;
      endpoint.x += taxel.force.x * kForceArrowScale;
      endpoint.y += taxel.force.y * kForceArrowScale;
      endpoint.z += taxel.force.z * kForceArrowScale;
      arrow.points.push_back(endpoint);
      markers.markers.push_back(std::move(arrow));
    }
  }

  sensors_.at(sensor_name).marker_publisher->publish(markers);
}

}  // namespace paxini_raw_controller
