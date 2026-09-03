import serial
import time
import serial.tools.list_ports
from typing import Optional, Dict, List
import logging
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext

# 配置日志系统
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# 传感器型号与分布力数据长度的映射（单位：字节）
SENSOR_DISTRIBUTED_LENGTH = {
    "S1813_elite": 93,
    "S2015_elite": 156,
    "S1813_core": 153,
    "S2716_core": 348,
    "S3013_core": 288,
    "M2826_omega": 381,
    "L3530_omega": 405,
    "S1610_elite": 75,
    "M2324_core": 204,
    "M3025_core": 231,
    "L5325_omega": 717,
    "M2020_elite": 27
}

class SensorUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Sensor Data Acquisition System")
        self.root.geometry("900x700")
        self.root.resizable(True, True)
        
        self.ser = None  # 串口对象
        self.running = False  # 数据采集状态
        self.init_ui()
        
    def init_ui(self):
        # ===== 配置区域 =====
        config_frame = ttk.LabelFrame(self.root, text="Device Configuration")
        config_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # COM口选择
        ttk.Label(config_frame, text="COM Port:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.com_var = tk.StringVar()
        self.com_combo = ttk.Combobox(config_frame, textvariable=self.com_var, width=10)
        self.com_combo.grid(row=0, column=1, padx=5, pady=5)
        ttk.Button(config_frame, text="Refresh", command=self.refresh_com_ports).grid(row=0, column=2, padx=5, pady=5)
        
        # 传感器型号选择（联动数据长度）
        ttk.Label(config_frame, text="Sensor Model:").grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        self.sensor_model = tk.StringVar(value="S1813_elite")
        self.sensor_combo = ttk.Combobox(config_frame, textvariable=self.sensor_model, width=15)
        self.sensor_combo['values'] = list(SENSOR_DISTRIBUTED_LENGTH.keys())
        self.sensor_combo.bind("<<ComboboxSelected>>", self.update_length_by_model)  # 选择型号时更新长度
        self.sensor_combo.grid(row=0, column=4, padx=5, pady=5)
        
        # 模组号选择（联动设备地址）
        ttk.Label(config_frame, text="Module ID:").grid(row=0, column=5, padx=5, pady=5, sticky=tk.W)
        self.module_id = tk.StringVar(value="00")  # 默认模组号00
        self.module_combo = ttk.Combobox(config_frame, textvariable=self.module_id, width=5)
        self.module_combo['values'] = ["00", "01", "02", "03", "04", "05", "06", "07"]
        self.module_combo.bind("<<ComboboxSelected>>", self.update_device_addr)  # 选择模组号时更新设备地址
        self.module_combo.grid(row=0, column=6, padx=5, pady=5)
        
        # 设备地址显示（只读，由模组号计算得出）
        ttk.Label(config_frame, text="Device Address:").grid(row=0, column=7, padx=5, pady=5, sticky=tk.W)
        self.device_addr_var = tk.StringVar(value="01")  # 模组号00对应设备地址01
        ttk.Label(config_frame, textvariable=self.device_addr_var, width=5).grid(row=0, column=8, padx=5, pady=5)
        
        # 数据长度设置（可手动修改）
        ttk.Label(config_frame, text="Distributed Force Length (bytes):").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.dist_length = tk.StringVar(value=str(SENSOR_DISTRIBUTED_LENGTH["S1813_elite"]))
        self.dist_entry = ttk.Entry(config_frame, textvariable=self.dist_length, width=10)
        self.dist_entry.grid(row=1, column=1, padx=5, pady=5)
        
        ttk.Label(config_frame, text="Resultant Force Length (bytes):").grid(row=1, column=2, padx=5, pady=5, sticky=tk.W)
        self.result_length = tk.StringVar(value="3")
        ttk.Entry(config_frame, textvariable=self.result_length, width=10).grid(row=1, column=3, padx=5, pady=5)
        
        # 控制按钮
        btn_frame = ttk.Frame(config_frame)
        btn_frame.grid(row=1, column=4, columnspan=5, padx=5, pady=5)
        ttk.Button(btn_frame, text="Connect", command=self.connect_device).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Calibrate Sensor", command=self.calibrate_sensor).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Start Collection", command=self.start_collection).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Stop Collection", command=self.stop_collection).pack(side=tk.LEFT, padx=5)
        
        # ===== 日志显示区域 =====
        log_frame = ttk.LabelFrame(self.root, text="Data Log")
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, state=tk.DISABLED)
        self.log_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # 初始化COM口列表和设备地址
        self.refresh_com_ports()
        self.update_device_addr()

    def refresh_com_ports(self):
        """刷新COM口列表"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.com_combo['values'] = ports
        if ports:
            self.com_var.set(ports[0])

    def update_device_addr(self, event=None):
        """根据模组号计算设备地址（设备地址 = 模组号 + 1，转为十六进制）"""
        try:
            module_dec = int(self.module_id.get(), 16)  # 模组号转为十进制
            device_dec = module_dec + 1  # 设备地址 = 模组号 + 1
            device_hex = f"{device_dec:02X}"  # 转为2位十六进制（大写）
            self.device_addr_var.set(device_hex)
            self.log(f"Module ID {self.module_id.get()} -> Device Address {device_hex}")
        except ValueError:
            self.log("Invalid module ID format; unable to calculate device address")

    def update_length_by_model(self, event=None):
        """根据选择的传感器型号自动更新分布力数据长度"""
        model = self.sensor_model.get()
        if model in SENSOR_DISTRIBUTED_LENGTH:
            self.dist_length.set(str(SENSOR_DISTRIBUTED_LENGTH[model]))
            self.log(f"Updated distributed force length for {model}: {SENSOR_DISTRIBUTED_LENGTH[model]} bytes")

    def log(self, message: str):
        """在UI中显示日志"""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def connect_device(self):
        """连接串口设备"""
        if self.ser and self.ser.is_open:
            self.ser.close()
        
        com_port = self.com_var.get()
        if not com_port:
            messagebox.showerror("Error", "Please select a COM port")
            return
        
        try:
            self.ser = serial.Serial(
                port=com_port,
                baudrate=921600,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,                    # 读取超时缩短至0.1秒
                write_timeout=0.1,              # 写入超时缩短
                inter_byte_timeout=0.0005,      # 字节间隔超时缩短
                xonxoff=False,
                rtscts=False
            )
            if self.ser.is_open:
                self.log(f"Connected to {com_port} at 921600 baud")
                logger.info(f"Connected to {com_port}")
        except serial.SerialException as e:
            self.log(f"Connection failed: {str(e)}")
            logger.error(f"Connection failed: {str(e)}")
            messagebox.showerror("Connection Failed", str(e))

    def get_commands(self) -> Dict[str, str]:
        """生成命令帧"""
        model = self.sensor_model.get()
        device_addr = self.device_addr_var.get()  # 设备地址（十六进制，如"01"）
        dist_len = int(self.dist_length.get())  # 分布力数据长度（十进制）
        
        # 1. 分布力命令帧：
        # 命令帧格式：55 AA 09 00 [设备地址] 00 FB 0E 04 00 00 [长度低8位] [长度高8位]
        len_low = dist_len & 0xFF  # 数据长度低8位
        len_high = (dist_len >> 8) & 0xFF  # 数据长度高8位
        len_hex = f"{len_low:02X}{len_high:02X}"  # 小端格式（低位+高位）
        distributed_frame = f"55 AA 09 00 {device_addr} 00 FB 0E 04 00 00 {len_hex[0:2]} {len_hex[2:4]}"
        
        # 2. 标定命令帧：
        # 格式：55 AA 0A 00 [设备地址] 00 79 03 00 00 00 01 00 01
        calibration_frame = f"55 AA 0A 00 {device_addr} 00 79 03 00 00 00 01 00 01"
        
        # 3. 合力命令帧：
        # 格式：55 AA 09 00 [设备地址] 00 FB F0 03 00 00 03 00
        resultant_frame = f"55 AA 09 00 {device_addr} 00 FB F0 03 00 00 03 00"
        
        return {
            "calibration": calibration_frame,
            "resultant_force": resultant_frame,
            "distributed_force": distributed_frame
        }

    def calculate_lrc(self, data: bytes) -> int:
        """计算LRC校验值"""
        lrc = 0
        for byte in data:
            lrc = (lrc + byte) & 0xFF
        lrc = ((~lrc) + 1) & 0xFF
        return lrc

    def send_command(self, command_type: str) -> Optional[bytes]:
        """发送命令并返回响应"""
        if not self.ser or not self.ser.is_open:
            self.log("No device connected. Please connect first.")
            return None
        
        commands = self.get_commands()
        if command_type not in commands:
            self.log(f"Unknown command type: {command_type}")
            return None
        
        # 生成带LRC的命令帧
        frame = commands[command_type].replace(" ", "")  # 移除空格
        try:
            frame_bytes = bytes.fromhex(frame)
            lrc = self.calculate_lrc(frame_bytes)
            frame_with_lrc = frame + f"{lrc:02X}"  # 追加LRC
        except ValueError as e:
            self.log(f"Failed to generate command frame: {str(e)}")
            return None
        
        # 发送命令并打印详情（含设备地址和数据长度）
        try:
            data_bytes = bytes.fromhex(frame_with_lrc)
            self.ser.write(data_bytes)
            print(f"发送{command_type}命令（设备地址={self.device_addr_var.get()}）: {frame_with_lrc}")
        except serial.SerialException as e:
            self.log(f"Failed to send command: {str(e)}")
            return None
        
        # 读取响应
        time.sleep(0.01)
        response = b""
        start_time = time.time()
        while time.time() - start_time < 0.2:  # 1.5秒超时
            if self.ser.in_waiting > 0:
                response += self.ser.read(self.ser.in_waiting)  # 一次性读取所有等待的数据，减少循环次数
                # 若已读取到预期长度（根据命令类型），提前退出
                if command_type == "resultant_force" and len(response) >= 14 + 3:
                    break
                if command_type == "distributed_force" and len(response) >= 14 + int(self.dist_length.get()):
                    break
            # 极短延迟
            time.sleep(0.0001)
            
        if response:
            print(f"收到响应（{len(response)}字节）: {response.hex()}")
            return response
        print("未收到响应")
        return None

    def calibrate_sensor(self):
        """标定"""
        response = self.send_command("calibration")
        if response:
            # 校验帧头
            if response[:2].hex() == "aa55":
                self.log(f"{self.sensor_model.get()} calibration successful")
            else:
                self.log("Invalid calibration response format")
        else:
            self.log("Calibration failed")

    def parse_resultant_force(self, data: bytes) -> Optional[Dict]:
        """解析合力数据"""
        if len(data) != int(self.result_length.get()):
            print(f"合力数据长度错误（预期{self.result_length.get()}字节，实际{len(data)}字节）")
            return None
        
        byte1, byte2, byte3 = data[0], data[1], data[2]
        val1 = byte1 if byte1 <= 127 else byte1 - 256
        val2 = byte2 if byte2 <= 127 else byte2 - 256
        val3 = byte3
        
        # 转换为物理单位
        force_x = val1 * 0.1
        force_y = val2 * 0.1
        force_z = val3 * 0.1
        
        return {
            "raw": (byte1, byte2, byte3),
            "parsed": (val1, val2, val3),
            "force_N": (force_x, force_y, force_z)
        }

    def parse_distributed_force(self, data: bytes) -> List[Dict]:
        """解析分布力数据"""
        parsed = []
        total_len = int(self.dist_length.get())
        if len(data) < total_len:
            print(f"分布力数据长度不足（预期{total_len}字节，实际{len(data)}字节）")
            total_len = len(data)
        
        group_count = total_len // 3  # 每3字节一组
        for i in range(group_count):
            byte1 = data[i*3]
            byte2 = data[i*3 + 1]
            byte3 = data[i*3 + 2]
            
            val1 = byte1 if byte1 <= 127 else byte1 - 256
            val2 = byte2 if byte2 <= 127 else byte2 - 256
            val3 = byte3
            
            force_x = val1 * 0.1
            force_y = val2 * 0.1
            force_z = val3 * 0.1
            
            parsed.append({
                "index": i,
                "raw": (byte1, byte2, byte3),
                "parsed": (val1, val2, val3),
                "force_N": (force_x, force_y, force_z)
            })
        return parsed

    def collect_data(self):
        """循环采集数据"""
        if not self.running or not self.ser or not self.ser.is_open:
            return
        
        # 读取合力数据
        result_response = self.send_command("resultant_force")
        if result_response and len(result_response) > 14:
            result_data = result_response[14:14 + int(self.result_length.get())]
            result_parsed = self.parse_resultant_force(result_data)
            if result_parsed:
                print("\n===== 合力数据 =====")
                print(f"合力数据原始字节: 0x{result_parsed['raw'][0]:02X}, 0x{result_parsed['raw'][1]:02X}, 0x{result_parsed['raw'][2]:02X}")
                print(f"合力数据解析值: X={result_parsed['parsed'][0]}, Y={result_parsed['parsed'][1]}, Z={result_parsed['parsed'][2]}")
                self.log(f"Resultant force: X={result_parsed['force_N'][0]:.1f}N, Y={result_parsed['force_N'][1]:.1f}N, Z={result_parsed['force_N'][2]:.1f}N")
                self.log(f"")
        
        # 读取分布力数据
        dist_response = self.send_command("distributed_force")
        if dist_response and len(dist_response) > 14:
            dist_data = dist_response[14:14 + int(self.dist_length.get())]
            dist_parsed = self.parse_distributed_force(dist_data)
            if dist_parsed:
                print("\n===== 分布力数据 =====")
                print(f"总测点: {len(dist_parsed)}")
                for point in dist_parsed:  
                    print(f"测点{point['index']:02d} | 物理值: X={point['force_N'][0]:.1f}N, Y={point['force_N'][1]:.1f}N, Z={point['force_N'][2]:.1f}N")
                print(f"------------------------------------------")
        
        # 继续循环采集,10ms间隔
        self.root.after(10, self.collect_data)

    def start_collection(self):
        """开始数据采集"""
        if self.running:
            self.log("Data collection is already running")
            return
        
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Warning", "Please connect a device first")
            return
        
        self.running = True
        self.log("Starting data collection...")
        self.collect_data()

    def stop_collection(self):
        """停止数据采集"""
        self.running = False
        self.log("Data collection stopped")

    def on_close(self):
        """关闭窗口时释放资源"""
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = SensorUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()



