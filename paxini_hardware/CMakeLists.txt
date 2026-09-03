cmake_minimum_required(VERSION 3.8)
project(paxini_hardware)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra -Werror=conversion -Werror=unused-but-set-variable -Werror=return-type -Werror=shadow)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/TactileTaxel.msg"
  "msg/TactileSensor.msg"
  DEPENDENCIES geometry_msgs std_msgs
)

add_library(paxini_tactile_hardware SHARED
  src/tactile_sensor.cpp
  src/taxel_geometry.cpp
  src/paxini_tactile_hardware.cpp
)
target_compile_features(paxini_tactile_hardware PUBLIC cxx_std_17)
target_include_directories(paxini_tactile_hardware PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(paxini_tactile_hardware ament_index_cpp hardware_interface pluginlib rclcpp)
target_link_libraries(paxini_tactile_hardware yaml-cpp)
pluginlib_export_plugin_description_file(hardware_interface paxini_hardware_plugin.xml)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})
install(DIRECTORY geometry DESTINATION share/${PROJECT_NAME})
install(TARGETS paxini_tactile_hardware
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_include_directories(include)
ament_export_libraries(paxini_tactile_hardware)
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(ament_index_cpp geometry_msgs hardware_interface pluginlib rclcpp rosidl_default_runtime std_msgs yaml-cpp)

# Needed so downstream packages (paxini_controller, paxini_raw_hardware,
# paxini_raw_controller) can link against this package's own generated
# message typesupport target.
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(paxini_tactile_hardware "${cpp_typesupport_target}")

if(BUILD_TESTING)
endif()

ament_package()
