import glob
import time
import psutil
import serial

PORT = "/dev/ttyUSB0"
BAUD = 115200

def get_gpu_load():
    paths = glob.glob("/sys/class/drm/card*/device/gpu_busy_percent")
    for path in paths:
        try:
            with open(path, "r") as f:
                value = int(f.read().strip())
            return max(0, min(value, 100))
        except (OSError, ValueError):
            pass
    return 0

def get_temperatures():
    cpu_temp = gpu_temp = nvme_temp = 0
    try:
        temps = psutil.sensors_temperatures()
        if "k10temp" in temps and temps["k10temp"]:
            cpu_temp = round(temps["k10temp"][0].current)
        if "amdgpu" in temps and temps["amdgpu"]:
            gpu_temp = round(temps["amdgpu"][0].current)
        if "nvme" in temps and temps["nvme"]:
            nvme_temp = round(temps["nvme"][0].current)
    except Exception:
        pass
    return cpu_temp, gpu_temp, nvme_temp

def get_fan_speed():
    try:
        fans = psutil.sensors_fans()
        if "dell_smm" in fans and fans["dell_smm"]:
            return round(fans["dell_smm"][0].current)
    except Exception:
        pass
    return 0

def get_battery():
    try:
        battery = psutil.sensors_battery()
        if battery is None:
            return 0, 0
        return round(battery.percent), 1 if battery.power_plugged else 0
    except Exception:
        return 0, 0

def get_uptime():
    try:
        return int(time.time() - psutil.boot_time())
    except Exception:
        return 0

def bytes_per_second(current, previous, elapsed):
    if elapsed <= 0:
        return 0
    difference = max(0, current - previous)
    return int(difference / elapsed)

print("======================================")
print("   LINUX -> ESP32 SYSTEM MONITOR V5")
print("======================================")
print(f"\nPuerto : {PORT}\nBaudios: {BAUD}\n")
print("Ctrl+C para salir\n")

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
except serial.SerialException as e:
    print("ERROR abriendo el puerto:", e)
    raise SystemExit(1)

print("Esperando al ESP32...")
time.sleep(2)
print("ESP32 conectado.\n")

psutil.cpu_percent(interval=None)
previous_net = psutil.net_io_counters()
previous_net_time = time.monotonic()

try:
    while True:
        cpu = round(psutil.cpu_percent(interval=0.5))
        ram = round(psutil.virtual_memory().percent)
        cpu_temp, gpu_temp, nvme_temp = get_temperatures()
        fan = get_fan_speed()
        gpu = get_gpu_load()
        disk_percent = round(psutil.disk_usage("/").percent)
        battery, plugged = get_battery()
        uptime = get_uptime()

        current_net = psutil.net_io_counters()
        current_net_time = time.monotonic()
        elapsed = current_net_time - previous_net_time

        download_bps = bytes_per_second(current_net.bytes_recv, previous_net.bytes_recv, elapsed)
        upload_bps = bytes_per_second(current_net.bytes_sent, previous_net.bytes_sent, elapsed)

        previous_net = current_net
        previous_net_time = current_net_time

        download_bps = min(download_bps, 999999999)
        upload_bps = min(upload_bps, 999999999)

        message = (
            f"C{cpu}|R{ram}|T{cpu_temp}|F{fan}|G{gpu}|GT{gpu_temp}"
            f"|D{disk_percent}|N{nvme_temp}|B{battery}|P{plugged}"
            f"|U{uptime}|RX{download_bps}|TX{upload_bps}\n"
        )

        try:
            ser.write(message.encode("ascii"))
            ser.flush()
        except serial.SerialException:
            print("\n\nESP32 desconectado.")
            break

        hours = uptime // 3600
        minutes = (uptime % 3600) // 60
        power_text = "AC" if plugged else "BAT"

        print(
            "\r"
            f"CPU {cpu:3d}% | RAM {ram:3d}% | TEMP {cpu_temp:2d}C | FAN {fan:4d}"
            f" | GPU {gpu:3d}% | GPU {gpu_temp:2d}C | SSD {disk_percent:3d}%"
            f" | NVME {nvme_temp:2d}C | BAT {battery:3d}% {power_text}"
            f" | DOWN {download_bps/1048576:6.2f} MB/s"
            f" | UP {upload_bps/1048576:6.2f} MB/s"
            f" | UPTIME {hours}h{minutes:02d}",
            end="", flush=True
        )

        time.sleep(0.5)

except KeyboardInterrupt:
    print("\n\nMonitor detenido.")
finally:
    if ser.is_open:
        ser.close()
    print("Puerto serial cerrado.")
