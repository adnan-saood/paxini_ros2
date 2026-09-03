from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import Command, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    description_file = PathJoinSubstitution([
        FindPackageShare("paxini_description"),
        "urdf",
        "paxini_tactile_sensors.urdf.xacro",
    ])
    rviz_config = PathJoinSubstitution([
        FindPackageShare("paxini_raw_bringup"),
        "rviz",
        "taxel_forces_raw.rviz",
    ])

    # Only used to publish the static TF tree (paxini_base_link ->
    # L5325_omega_link / S1813_elite_link) referenced by the marker/message
    # frame_ids below. The <ros2_control> block in this same xacro file is
    # not used here since this pipeline intentionally bypasses
    # ros2_control/controller_manager entirely.
    robot_description = {
        "robot_description": Command(["xacro ", description_file])
    }

    return LaunchDescription([
        DeclareLaunchArgument("serial_port", default_value="/dev/ttyACM0"),
        DeclareLaunchArgument("baud_rate", default_value="921600"),
        DeclareLaunchArgument("publish_rate_hz", default_value="50.0"),
        DeclareLaunchArgument("rviz", default_value="true"),
        Node(
            package="robot_state_publisher",
            executable="robot_state_publisher",
            name="paxini_raw_state_publisher",
            output="screen",
            parameters=[robot_description],
        ),
        # Sole owner of the tactile sensor serial bus. Reads both resultant
        # and full per-taxel data and publishes them on fixed absolute
        # topics, entirely outside of ros2_control/controller_manager.
        Node(
            package="paxini_raw_hardware",
            executable="paxini_raw_hardware_node",
            name="paxini_raw_hardware_node",
            output="screen",
            parameters=[{
                "serial_port": LaunchConfiguration("serial_port"),
                "baud_rate": LaunchConfiguration("baud_rate"),
                "publish_rate_hz": LaunchConfiguration("publish_rate_hz"),
            }],
        ),
        # Enriches the raw per-taxel data with geometry and publishes the
        # full TactileSensor message + RViz MarkerArray.
        Node(
            package="paxini_raw_controller",
            executable="paxini_raw_controller_node",
            name="paxini_raw_controller_node",
            output="screen",
        ),
        Node(
            condition=IfCondition(LaunchConfiguration("rviz")),
            package="rviz2",
            executable="rviz2",
            name="paxini_raw_rviz",
            output="screen",
            arguments=["-d", rviz_config],
        ),
    ])
