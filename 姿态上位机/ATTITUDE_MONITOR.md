# 姿态串口上位机

主机 STM32 使用 USART2（460800、8N1）连接 USB-TTL：

- 主机 PA2 / USART2_TX 接 USB-TTL 的 RXD。
- 主机 PA3 / USART2_RX 接 USB-TTL 的 TXD。
- 两边 GND 相连；USB-TTL 必须使用 3.3V TTL 电平，不要把 5V TX 接到 STM32。

USART2 向电脑转发带 CRC 的 29 字节姿态帧。电脑端检查 CRC，解出四元数后计算 Pitch、Roll、Yaw 并实时绘图。界面中的 `K` 是互补滤波陀螺仪权重；点击“下发权重”后，主机通过板间 USART1 将设置发给从机。

在此文件夹打开 PowerShell 并运行：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-attitude-monitor.txt
.\.venv\Scripts\python.exe .\attitude_monitor.py
```

在 VS Code 中单独打开此文件夹，并选择 `.venv/Scripts/python.exe` 作为 Python 解释器。

在界面选择 USB-TTL 对应的 COM 口并连接。调节权重范围为 0.900–0.999；数值越大越依赖陀螺仪，越小越依赖加速度计。
