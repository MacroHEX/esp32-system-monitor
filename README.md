# ESP32 Linux System Monitor

A compact real-time Linux hardware monitor built with an **Ideaspark ESP32-WROOM-32**
and its integrated **128×64 SSD1306 bicolor OLED**.

The Linux host collects system metrics with Python and sends them to the ESP32 over
USB serial. The ESP32 rotates through four dashboard pages and uses its onboard LED
as a visual load warning.

## Features

- CPU usage
- RAM usage
- CPU temperature
- Fan RPM
- AMD GPU usage
- GPU temperature
- GPU history graph
- Root disk usage
- NVMe temperature
- Battery percentage and AC/BAT status
- System uptime
- Network download/upload speed
- Network traffic history graph
- Hardware LED warning/critical alerts
- USB/offline indicator

## Hardware

- Ideaspark ESP32-WROOM-32
- Integrated SSD1306 OLED, 128×64
- OLED SDA: GPIO 21
- OLED SCL: GPIO 22
- OLED I2C address: `0x3C`
- Onboard/extra LED: GPIO 2
- USB serial: 115200 baud

## Repository structure

```text
linux-esp32-system-monitor/
├── README.md
├── LICENSE
├── .gitignore
├── arduino/
│   └── esp32_linux_monitor/
│       └── esp32_linux_monitor.ino
├── host/
│   └── linux-monitor.py
└── docs/
    └── images/
```

## Arduino dependencies

Install these libraries from Arduino Library Manager:

- Adafruit GFX Library
- Adafruit SSD1306

Open:

`arduino/esp32_linux_monitor/esp32_linux_monitor.ino`

The sketch folder and the main `.ino` intentionally have the same name for Arduino IDE compatibility.

## Linux dependencies

On Arch Linux:

```bash
sudo pacman -S python-pyserial python-psutil
```

The default serial device is:

```text
/dev/ttyUSB0
```

Run the host monitor with:

```bash
python host/linux-monitor.py
```

## OLED pages

The display automatically rotates every five seconds:

`SYSTEM → GPU → STORAGE → NETWORK`

## LED alerts

The GPIO 2 LED acts as a load indicator.

**Warning — slow blink**

- CPU ≥ 80%
- RAM ≥ 85%
- Fan ≥ 4500 RPM
- Total network traffic ≥ 10 MB/s

**Critical — fast blink**

- CPU ≥ 95%
- RAM ≥ 90%
- Fan ≥ 5200 RPM
- Total network traffic ≥ 30 MB/s

Normal operation keeps the LED off.

## Serial protocol

Example packet:

```text
C5|R55|T70|F2900|G3|GT51|D17|N43|B100|P1|U20520|RX1832440|TX245322
```

`RX` and `TX` are bytes per second.

## Notes

Sensor availability depends on the Linux hardware and drivers. This version was
developed around an AMD GPU, `k10temp`, NVMe temperature sensors, and a Dell fan
reported through `dell_smm`.

Do not commit Wi-Fi passwords, API keys, access tokens, or other secrets if you
extend the project later.
