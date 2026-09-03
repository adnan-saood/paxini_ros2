from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("serial_port", default_value="/dev/ttyUSB0"),
        DeclareLaunchArgument("baud_rate", default_value="921600"),
        DeclareLaunchArgument("poll_period_ms", default_value="20"),
        Node(
            package="paxini_controller",
            executable="paxini_tactile_controller",
            name="paxini_tactile_controller",
            output="screen",
            parameters=[{
                "serial_port": LaunchConfiguration("serial_port"),
                "baud_rate": LaunchConfiguration("baud_rate"),
                "poll_period_ms": LaunchConfiguration("poll_period_ms"),
            }],
        ),
    ])
