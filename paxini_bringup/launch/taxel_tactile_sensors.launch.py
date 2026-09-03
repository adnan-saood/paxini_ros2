from launch import LaunchDescription
from launch.actions import LogInfo
from launch.substitutions import Command, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # NOTE: this bringup only demonstrates claiming the lightweight resultant
    # force via ros2_control (6 scalar state interfaces total, 3 per
    # sensor). It does NOT own the sensor's serial port and does not
    # visualize per-taxel data -- PaxiniTactileHardware only subscribes to
    # the resultant-force topic published by paxini_raw_hardware.
    #
    # For the full-resolution pipeline (raw per-taxel data, RViz taxel/force
    # marker visualization, and the actual serial_port/baud_rate
    # configuration), launch paxini_raw_bringup's
    # paxini_raw_tactile_sensors.launch.py alongside this file, e.g.:
    #   ros2 launch paxini_bringup taxel_tactile_sensors.launch.py
    #   ros2 launch paxini_raw_bringup paxini_raw_tactile_sensors.launch.py serial_port:=/dev/ttyUSB0
    description_file = PathJoinSubstitution([
        FindPackageShare("paxini_description"),
        "urdf",
        "paxini_tactile_sensors.urdf.xacro",
    ])
    controllers_file = PathJoinSubstitution([
        FindPackageShare("paxini_bringup"),
        "config",
        "paxini_controllers.yaml",
    ])

    robot_description = {
        "robot_description": Command(["xacro ", description_file])
    }

    return LaunchDescription([
        LogInfo(
            msg=(
                "paxini_bringup only claims the resultant force via "
                "ros2_control as an example; launch paxini_raw_bringup for "
                "the serial port configuration and full per-taxel RViz "
                "visualization."
            )
        ),
        Node(
            package="controller_manager",
            executable="ros2_control_node",
            name="controller_manager",
            output="screen",
            parameters=[robot_description, controllers_file],
        ),
        Node(
            package="robot_state_publisher",
            executable="robot_state_publisher",
            name="paxini_tactile_state_publisher",
            output="screen",
            parameters=[robot_description],
        ),
        Node(
            package="controller_manager",
            executable="spawner",
            arguments=["tactile_broadcaster", "--controller-manager", "/controller_manager"],
            output="screen",
        ),
    ])
