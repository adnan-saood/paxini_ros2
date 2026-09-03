cmake_minimum_required(VERSION 3.8)
project(paxini_raw_hardware)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra -Werror=conversion -Werror=unused-but-set-variable -Werror=return-type -Werror=shadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(paxini_hardware REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)

add_library(paxini_raw_hardware_node SHARED src/tactile_raw_hardware_node.cpp)
target_compile_features(paxini_raw_hardware_node PUBLIC cxx_std_17)
target_include_directories(paxini_raw_hardware_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(paxini_raw_hardware_node geometry_msgs paxini_hardware rclcpp std_msgs std_srvs)

add_executable(paxini_raw_hardware_node_exe src/main.cpp)
target_compile_features(paxini_raw_hardware_node_exe PUBLIC cxx_std_17)
target_link_libraries(paxini_raw_hardware_node_exe paxini_raw_hardware_node)
ament_target_dependencies(paxini_raw_hardware_node_exe rclcpp)
set_target_properties(paxini_raw_hardware_node_exe PROPERTIES OUTPUT_NAME paxini_raw_hardware_node)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS paxini_raw_hardware_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS paxini_raw_hardware_node_exe DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
endif()

ament_package()
