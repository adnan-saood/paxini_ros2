cmake_minimum_required(VERSION 3.8)
project(paxini_controller)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra -Werror=conversion -Werror=unused-but-set-variable -Werror=return-type -Werror=shadow)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(controller_interface REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(paxini_hardware REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)

add_library(paxini_tactile_broadcaster SHARED src/tactile_broadcaster.cpp)
target_compile_features(paxini_tactile_broadcaster PUBLIC cxx_std_17)
target_include_directories(paxini_tactile_broadcaster PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(
  paxini_tactile_broadcaster
  controller_interface
  geometry_msgs
  paxini_hardware
  pluginlib
  rclcpp
)
pluginlib_export_plugin_description_file(controller_interface paxini_controller_plugin.xml)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS paxini_tactile_broadcaster ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)

if(BUILD_TESTING)
endif()

ament_package()
