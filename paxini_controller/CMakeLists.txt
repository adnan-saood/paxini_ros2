cmake_minimum_required(VERSION 3.8)
project(paxini_controller)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra -Werror=conversion -Werror=unused-but-set-variable -Werror=return-type -Werror=shadow)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(paxini_hardware REQUIRED)

add_executable(paxini_tactile_controller src/tactile_controller.cpp)
target_compile_features(paxini_tactile_controller PUBLIC cxx_std_17)
ament_target_dependencies(paxini_tactile_controller rclcpp paxini_hardware)
target_link_libraries(
  paxini_tactile_controller
  paxini_hardware::paxini_hardware__rosidl_typesupport_cpp
)

install(TARGETS paxini_tactile_controller DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
endif()

ament_package()
