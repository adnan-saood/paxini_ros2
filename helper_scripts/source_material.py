import queue
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from tkinter import messagebox, scrolledtext, ttk
from typing import Dict, List, Optional, Tuple

import serial
import serial.tools.list_ports


@dataclass(frozen=True)
class SensorConfig:
    name: str
    module_id: int
    distributed_length: int

    @property
    def device_address(self) -> int:
        return self.module_id + 1

    @property
    def point_count(self) -> int:
        return self.distributed_length // 3


SENSORS = (
    SensorConfig("L5325_omega", module_id=0, distributed_length=717),
    SensorConfig("S1813_elite", module_id=2, distributed_length=93),
)


def calculate_lrc(data: bytes) -> int:
    return (-sum(data)) & 0xFF


def build_command(sensor: SensorConfig, command: str) -> bytes:
    address = sensor.device_address
    if command == "resultant":
        frame = f"55 AA 09 00 {address:02X} 00 FB F0 03 00 00 03 00"
    elif command == "distributed":
        length_low = sensor.distributed_length & 0xFF
        length_high = sensor.distributed_length >> 8
        frame = (
            f"55 AA 09 00 {address:02X} 00 FB 0E 04 00 00 "
            f"{length_low:02X} {length_high:02X}"
        )
    else:
        raise ValueError(f"Unsupported command: {command}")

    frame_bytes = bytes.fromhex(frame)
    return frame_bytes + bytes([calculate_lrc(frame_bytes)])


def parse_force_values(data: bytes) -> List[Tuple[float, float, float]]:
    values = []
    for index in range(0, len(data) - 2, 3):
        x_raw, y_raw, z_raw = data[index : index + 3]
        x = (x_raw if x_raw <= 127 else x_raw - 256) * 0.1
        y = (y_raw if y_raw <= 127 else y_raw - 256) * 0.1
        z = z_raw * 0.1
        values.append((x, y, z))
    return values


class SensorPanel(ttk.LabelFrame):
    def __init__(self, parent: tk.Misc, sensor: SensorConfig):
        super().__init__(parent, text=f"{sensor.name} | Module {sensor.module_id:02X} | Address {sensor.device_address:02X}")
        self.sensor = sensor
        self.resultant_vars = {axis: tk.StringVar(value="--.- N") for axis in "XYZ"}
        self.status_var = tk.StringVar(value="Waiting for data")
        self.last_update_var = tk.StringVar(value="Last update: --")

        resultant_frame = ttk.Frame(self)
        resultant_frame.pack(fill=tk.X, padx=8, pady=(8, 4))
        for column, axis in enumerate("XYZ"):
            ttk.Label(resultant_frame, text=f"{axis} Force").grid(row=0, column=column, padx=12)
            ttk.Label(resultant_frame, textvariable=self.resultant_vars[axis], font=("Segoe UI", 14, "bold")).grid(
                row=1, column=column, padx=12
            )

        ttk.Label(self, textvariable=self.status_var).pack(anchor=tk.W, padx=8, pady=(4, 0))
        ttk.Label(self, textvariable=self.last_update_var).pack(anchor=tk.W, padx=8, pady=(0, 4))

        self.data_text = scrolledtext.ScrolledText(self, height=12, wrap=tk.NONE, state=tk.DISABLED, font=("Consolas", 9))
        self.data_text.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

    def show_data(
        self,
        resultant: Optional[Tuple[float, float, float]],
        distributed: List[Tuple[float, float, float]],
    ) -> None:
        if resultant is not None:
            for axis, value in zip("XYZ", resultant):
                self.resultant_vars[axis].set(f"{value:.1f} N")

        self.status_var.set(f"Distributed readings: {len(distributed)} / {self.sensor.point_count} points")
        self.last_update_var.set(f"Last update: {time.strftime('%H:%M:%S')}")
        lines = ["Point       X (N)       Y (N)       Z (N)", "------------------------------------------"]
        lines.extend(
            f"{index:>5}  {x:>10.1f}  {y:>10.1f}  {z:>10.1f}"
            for index, (x, y, z) in enumerate(distributed, start=1)
        )
        self.data_text.config(state=tk.NORMAL)
        self.data_text.delete("1.0", tk.END)
        self.data_text.insert(tk.END, "\n".join(lines))
        self.data_text.config(state=tk.DISABLED)

    def show_error(self, message: str) -> None:
        self.status_var.set(f"Error: {message}")


class DualSensorUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Dual Sensor Data Acquisition")
        self.root.geometry("1100x720")
        self.root.minsize(850, 550)
        self.serial_port: Optional[serial.Serial] = None
        self.worker: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        self.updates: queue.Queue[Tuple[str, SensorConfig, object]] = queue.Queue()

        self.com_port_var = tk.StringVar()
        self.connection_var = tk.StringVar(value="Disconnected")
        self.panels: Dict[str, SensorPanel] = {}
        self.build_ui()
        self.refresh_ports()
        self.root.after(100, self.process_updates)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def build_ui(self) -> None:
        controls = ttk.Frame(self.root, padding=10)
        controls.pack(fill=tk.X)
        ttk.Label(controls, text="COM Port:").pack(side=tk.LEFT)
        self.com_ports = ttk.Combobox(controls, textvariable=self.com_port_var, state="readonly", width=14)
        self.com_ports.pack(side=tk.LEFT, padx=(6, 4))
        ttk.Button(controls, text="Refresh", command=self.refresh_ports).pack(side=tk.LEFT, padx=4)
        ttk.Button(controls, text="Connect", command=self.connect).pack(side=tk.LEFT, padx=4)
        ttk.Button(controls, text="Start", command=self.start).pack(side=tk.LEFT, padx=4)
        ttk.Button(controls, text="Stop", command=self.stop).pack(side=tk.LEFT, padx=4)
        ttk.Label(controls, textvariable=self.connection_var).pack(side=tk.RIGHT)

        sensor_area = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        sensor_area.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))
        for sensor in SENSORS:
            panel = SensorPanel(sensor_area, sensor)
            sensor_area.add(panel, weight=1)
            self.panels[sensor.name] = panel

    def refresh_ports(self) -> None:
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.com_ports["values"] = ports
        if ports and self.com_port_var.get() not in ports:
            self.com_port_var.set(ports[0])

    def connect(self) -> None:
        self.stop()
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        port_name = self.com_port_var.get()
        if not port_name:
            messagebox.showerror("Error", "Please select a COM port")
            return
        try:
            self.serial_port = serial.Serial(
                port=port_name,
                baudrate=921600,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.05,
                write_timeout=0.2,
                inter_byte_timeout=0.0005,
            )
            self.connection_var.set(f"Connected: {port_name} at 921600 baud")
        except serial.SerialException as error:
            self.serial_port = None
            self.connection_var.set("Disconnected")
            messagebox.showerror("Connection Failed", str(error))

    def start(self) -> None:
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showwarning("Warning", "Connect to a COM port first")
            return
        if self.worker and self.worker.is_alive():
            return
        self.stop_event.clear()
        self.worker = threading.Thread(target=self.poll_sensors, daemon=True)
        self.worker.start()
        self.connection_var.set(f"Collecting from {self.com_port_var.get()}")

    def stop(self) -> None:
        self.stop_event.set()
        if self.serial_port and self.serial_port.is_open:
            self.connection_var.set(f"Connected: {self.com_port_var.get()}")

    def read_response(self, expected_payload_length: int) -> Optional[bytes]:
        if not self.serial_port:
            return None
        response = bytearray()
        deadline = time.monotonic() + 0.25
        expected_total_length = 14 + expected_payload_length
        while time.monotonic() < deadline and not self.stop_event.is_set():
            waiting = self.serial_port.in_waiting
            if waiting:
                response.extend(self.serial_port.read(waiting))
                if len(response) >= expected_total_length:
                    return bytes(response)
            else:
                time.sleep(0.002)
        return bytes(response) if response else None

    def request_data(self, sensor: SensorConfig, command: str, payload_length: int) -> Optional[bytes]:
        if not self.serial_port:
            return None
        self.serial_port.reset_input_buffer()
        self.serial_port.write(build_command(sensor, command))
        response = self.read_response(payload_length)
        if response is None:
            raise TimeoutError(f"No {command} response")
        if response[:2] != b"\xaa\x55":
            raise ValueError(f"Invalid response header: {response[:2].hex()}")
        if len(response) < 14 + payload_length:
            raise ValueError(f"Incomplete {command} response: {len(response)} bytes")
        return response[14 : 14 + payload_length]

    def poll_sensors(self) -> None:
        while not self.stop_event.is_set():
            for sensor in SENSORS:
                if self.stop_event.is_set():
                    break
                try:
                    resultant_data = self.request_data(sensor, "resultant", 3)
                    distributed_data = self.request_data(sensor, "distributed", sensor.distributed_length)
                    resultant = parse_force_values(resultant_data)[0] if resultant_data else None
                    distributed = parse_force_values(distributed_data or b"")
                    self.updates.put(("data", sensor, (resultant, distributed)))
                except (serial.SerialException, TimeoutError, ValueError) as error:
                    self.updates.put(("error", sensor, str(error)))
            time.sleep(0.02)

    def process_updates(self) -> None:
        try:
            while True:
                kind, sensor, payload = self.updates.get_nowait()
                panel = self.panels[sensor.name]
                if kind == "data":
                    resultant, distributed = payload
                    panel.show_data(resultant, distributed)
                else:
                    panel.show_error(str(payload))
        except queue.Empty:
            pass
        self.root.after(100, self.process_updates)

    def on_close(self) -> None:
        self.stop()
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.root.destroy()


if __name__ == "__main__":
    app_root = tk.Tk()
    DualSensorUI(app_root)
    app_root.mainloop()