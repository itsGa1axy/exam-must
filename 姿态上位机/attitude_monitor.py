"""简易串口姿态波形上位机：显示 Pitch/Roll/Yaw，并下发互补滤波权重。"""

from __future__ import annotations

import queue
import math
import threading
import tkinter as tk
from collections import deque
from tkinter import messagebox, ttk

import serial
from serial.tools import list_ports


BAUD_RATE = 460800
HISTORY_SIZE = 500
ANGLE_RANGE_DEG = 180.0
TRACE_COLORS = ("#ef5350", "#66bb6a", "#42a5f5")
TRACE_NAMES = ("Pitch", "Roll", "Yaw")


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def decode_attitude_frame(frame: bytes) -> tuple[int, float, float, float] | None:
    if (len(frame) != 29 or frame[2] != 2 or frame[3] != 1 or frame[6] != 20
            or crc16_ccitt_false(frame[2:-2]) != int.from_bytes(frame[-2:], "little")):
        return None
    timestamp = int.from_bytes(frame[7:11], "little")
    q = [int.from_bytes(frame[11 + 4 * index:15 + 4 * index], "little", signed=True) / 1073741824.0
         for index in range(4)]
    w, x, y, z = q
    sin_pitch = max(-1.0, min(1.0, 2.0 * (w * y - z * x)))
    roll = math.atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y))
    pitch = math.asin(sin_pitch)
    yaw = math.atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))
    return timestamp, math.degrees(pitch), math.degrees(roll), math.degrees(yaw)


class AttitudeMonitor:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("姿态角实时监视")
        self.root.geometry("1000x650")
        self.serial_port: serial.Serial | None = None
        self.reader_thread: threading.Thread | None = None
        self.stop_event = threading.Event()
        self.data_queue: queue.Queue[tuple[int, float, float, float]] = queue.Queue()
        self.times: deque[int] = deque(maxlen=HISTORY_SIZE)
        self.angles = [deque(maxlen=HISTORY_SIZE) for _ in range(3)]

        self._build_controls()
        self._build_plot()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(30, self._refresh_plot)

    def _build_controls(self) -> None:
        controls = ttk.Frame(self.root, padding=8)
        controls.pack(fill=tk.X)

        ttk.Label(controls, text="串口").pack(side=tk.LEFT)
        self.port_box = ttk.Combobox(controls, width=18, state="readonly")
        self.port_box.pack(side=tk.LEFT, padx=(4, 6))
        ttk.Button(controls, text="刷新", command=self._refresh_ports).pack(side=tk.LEFT)
        self.connect_button = ttk.Button(controls, text="连接", command=self._toggle_connection)
        self.connect_button.pack(side=tk.LEFT, padx=6)
        self.status_label = ttk.Label(controls, text="未连接")
        self.status_label.pack(side=tk.LEFT, padx=10)
        self._refresh_ports()

        weight_frame = ttk.Frame(self.root, padding=(8, 0, 8, 8))
        weight_frame.pack(fill=tk.X)
        ttk.Label(weight_frame, text="陀螺仪权重 K（0.900–0.999）").pack(side=tk.LEFT)
        self.weight = tk.DoubleVar(value=0.980)
        self.weight_scale = ttk.Scale(
            weight_frame, from_=0.900, to=0.999, variable=self.weight,
            orient=tk.HORIZONTAL, command=self._weight_changed, length=280
        )
        self.weight_scale.pack(side=tk.LEFT, padx=10)
        self.weight_label = ttk.Label(weight_frame, text="0.980")
        self.weight_label.pack(side=tk.LEFT)
        self.send_button = ttk.Button(
            weight_frame, text="下发权重", command=self._send_weight, state=tk.DISABLED
        )
        self.send_button.pack(side=tk.LEFT, padx=12)
        ttk.Label(weight_frame, text="K 越大越依赖陀螺仪，越小越依赖加速度计。") \
            .pack(side=tk.LEFT, padx=8)

    def _build_plot(self) -> None:
        self.canvas = tk.Canvas(self.root, background="#101820", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

    def _refresh_ports(self) -> None:
        ports = [port.device for port in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and not self.port_box.get():
            self.port_box.current(0)

    def _toggle_connection(self) -> None:
        if self.serial_port is not None:
            self.close_serial()
            return
        port_name = self.port_box.get()
        if not port_name:
            messagebox.showwarning("串口未选择", "请先选择 USB-TTL 对应的串口。")
            return
        try:
            self.serial_port = serial.Serial(port_name, BAUD_RATE, timeout=0.1)
        except (OSError, serial.SerialException) as exc:
            messagebox.showerror("串口打开失败", str(exc))
            self.serial_port = None
            return

        # Each connection gets its own event so an old reader cannot be
        # reactivated when the user disconnects and reconnects quickly.
        self.stop_event = threading.Event()
        self.reader_thread = threading.Thread(target=self._read_serial, daemon=True)
        self.reader_thread.start()
        self.connect_button.configure(text="断开")
        self.send_button.configure(state=tk.NORMAL)
        self.status_label.configure(text=f"已连接 {port_name} @ {BAUD_RATE}")

    def _read_serial(self) -> None:
        port = self.serial_port
        if port is None:
            return
        stop_event = self.stop_event
        buffer = bytearray()
        while not stop_event.is_set():
            try:
                waiting = port.in_waiting
                buffer.extend(port.read(max(waiting, 1)))
                while True:
                    start = buffer.find(b"\xAA\x55")
                    if start < 0:
                        keep_sof = buffer[-1:] == b"\xAA"
                        buffer[:] = b"\xAA" if keep_sof else b""
                        break
                    if start > 0:
                        del buffer[:start]
                    if len(buffer) < 7:
                        break
                    payload_length = buffer[6]
                    if payload_length != 20:
                        del buffer[0]
                        continue
                    frame_length = 9 + payload_length
                    if len(buffer) < frame_length:
                        break
                    frame = bytes(buffer[:frame_length])
                    del buffer[:frame_length]
                    sample = decode_attitude_frame(frame)
                    if sample is not None:
                        self.data_queue.put(sample)
            except (OSError, ValueError, serial.SerialException):
                if not stop_event.is_set():
                    self.root.after(0, self._handle_serial_error, port, stop_event)
                break

    def _handle_serial_error(self, port: serial.Serial, stop_event: threading.Event) -> None:
        """Reset the UI only when the failed reader is still the active one."""
        if self.serial_port is not port or self.stop_event is not stop_event:
            return
        stop_event.set()
        try:
            port.close()
        except (OSError, serial.SerialException):
            pass
        self.serial_port = None
        self.reader_thread = None
        self.connect_button.configure(text="连接")
        self.send_button.configure(state=tk.DISABLED)
        self.status_label.configure(text="串口读取异常")

    def _send_weight(self) -> None:
        if self.serial_port is None or not self.serial_port.is_open:
            return
        value = self.weight.get()
        try:
            self.serial_port.write(f"K,{value:.3f}\n".encode("ascii"))
            self.status_label.configure(text=f"已下发 K={value:.3f}")
        except (OSError, serial.SerialException) as exc:
            messagebox.showerror("下发失败", str(exc))

    def _weight_changed(self, _value: str) -> None:
        self.weight_label.configure(text=f"{self.weight.get():.3f}")

    def _refresh_plot(self) -> None:
        while True:
            try:
                timestamp, pitch, roll, yaw = self.data_queue.get_nowait()
            except queue.Empty:
                break
            self.times.append(timestamp)
            for series, angle in zip(self.angles, (pitch, roll, yaw)):
                series.append(angle)

        self._draw_plot()
        self.root.after(30, self._refresh_plot)

    def _draw_plot(self) -> None:
        self.canvas.delete("all")
        width = max(self.canvas.winfo_width(), 400)
        height = max(self.canvas.winfo_height(), 300)
        left, right, top, bottom = 58, width - 18, 18, height - 34
        plot_width = right - left
        plot_height = bottom - top

        for line_index in range(7):
            y = top + plot_height * line_index / 6
            degree = ANGLE_RANGE_DEG - 2 * ANGLE_RANGE_DEG * line_index / 6
            self.canvas.create_line(left, y, right, y, fill="#34434d")
            self.canvas.create_text(left - 8, y, text=f"{degree:.0f}°", anchor=tk.E,
                                    fill="#c7d0d8", font=("Segoe UI", 9))
        self.canvas.create_line(left, top, left, bottom, fill="#82909b")
        self.canvas.create_line(left, bottom, right, bottom, fill="#82909b")

        for index, (name, color, series) in enumerate(
            zip(TRACE_NAMES, TRACE_COLORS, self.angles)
        ):
            self.canvas.create_line(left + 12 + index * 120, 14,
                                    left + 32 + index * 120, 14, fill=color, width=2)
            latest = series[-1] if series else 0.0
            self.canvas.create_text(left + 38 + index * 120, 14,
                                    text=f"{name}: {latest:7.2f}°", anchor=tk.W,
                                    fill=color, font=("Segoe UI", 10, "bold"))
            if len(series) < 2:
                continue
            points: list[float] = []
            for sample_index, value in enumerate(series):
                x = left + plot_width * sample_index / max(len(series) - 1, 1)
                bounded = max(-ANGLE_RANGE_DEG, min(ANGLE_RANGE_DEG, value))
                y = top + (ANGLE_RANGE_DEG - bounded) * plot_height / (2 * ANGLE_RANGE_DEG)
                points.extend((x, y))
            self.canvas.create_line(*points, fill=color, width=1.5, smooth=False)

        if self.times:
            self.canvas.create_text(left, height - 12,
                                    text=f"样本时间戳：{self.times[0]} → {self.times[-1]} ms",
                                    anchor=tk.W, fill="#c7d0d8", font=("Segoe UI", 9))

    def close_serial(self) -> None:
        self.stop_event.set()
        port = self.serial_port
        if port is not None:
            try:
                port.close()
            except (OSError, serial.SerialException):
                pass
        self.serial_port = None
        self.reader_thread = None
        self.connect_button.configure(text="连接")
        self.send_button.configure(state=tk.DISABLED)
        self.status_label.configure(text="未连接")

    def close(self) -> None:
        self.close_serial()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    AttitudeMonitor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
