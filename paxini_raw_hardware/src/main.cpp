#include <memory>

#include "paxini_raw_hardware/tactile_raw_hardware_node.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<paxini_raw_hardware::TactileRawHardwareNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
