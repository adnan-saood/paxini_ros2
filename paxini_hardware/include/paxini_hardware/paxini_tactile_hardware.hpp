#ifndef PAXINI_HARDWARE__PAXINI_TACTILE_HARDWARE_HPP_
#define PAXINI_HARDWARE__PAXINI_TACTILE_HARDWARE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "hardware_interface/sensor_interface.hpp"
#include "rclcpp/rclcpp.hpp"
#include "paxini_hardware/tactile_sensor.hpp"

namespace paxini_hardware
{

/// Lightweight ros2_control bridge that exposes only the resultant
/// (summary) force per sensor as scalar state interfaces.
///
/// This component does **not** open the serial port itself: the full-rate,
/// full-resolution reading of the tactile bus (resultant + per-taxel data)
/// is owned exclusively by the paxini_raw_hardware node, which publishes the
/// resultant force on a fixed absolute topic
/// (`/paxini/<sensor>/resultant_force_raw`). This class simply subscribes to
/// that topic on an internal node/executor thread and mirrors the latest
/// values into the 6 state interfaces claimed here, so it can serve as a
/// minimal example of integrating a small, well-scoped sensor reading with
/// ros2_control without recreating the hundreds-of-interfaces problem that
/// comes from exposing every taxel individually.
class PaxiniTactileHardware : public hardware_interface::SensorInterface
{
public:
  hardware_interface::CallbackReturn on_init(const hardware_interface::HardwareInfo & info) override;
  hardware_interface::CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override;
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  struct CachedForce
  {
    std::atomic<double> x{0.0};
    std::atomic<double> y{0.0};
    std::atomic<double> z{0.0};
  };

  std::vector<double> state_values_;
  std::unordered_map<std::string, std::size_t> state_indices_;
  std::unordered_map<std::string, std::unique_ptr<CachedForce>> cached_forces_;

  rclcpp::Node::SharedPtr node_;
  rclcpp::executors::SingleThreadedExecutor::SharedPtr executor_;
  std::thread spin_thread_;
  std::vector<rclcpp::Subscription<geometry_msgs::msg::Vector3Stamped>::SharedPtr> subscriptions_;

  void stop_executor();
};

}  // namespace paxini_hardware

#endif  // PAXINI_HARDWARE__PAXINI_TACTILE_HARDWARE_HPP_
