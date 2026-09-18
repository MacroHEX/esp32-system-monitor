import glob
import os
import re
import subprocess
import time

import psutil
import serial

PORT = "/dev/ttyUSB0"
BAUD = 115200

WAKATIME_CLI = os.path.expanduser(
    "~/.wakatime/wakatime-cli-linux-amd64"
)

WAKATIME_REFRESH = 60
DEV_REFRESH = 10


# ============================================================
# SYSTEM
# ============================================================

def get_gpu_load():
    paths = glob.glob(
        "/sys/class/drm/card*/device/gpu_busy_percent"
    )

    for path in paths:
        try:
            with open(path, "r") as f:
                value = int(f.read().strip())

            return max(0, min(value, 100))

        except (OSError, ValueError):
            pass

    return 0


def get_temperatures():
    cpu_temp = 0
    gpu_temp = 0
    nvme_temp = 0

    try:
        temps = psutil.sensors_temperatures()

        if "k10temp" in temps and temps["k10temp"]:
            cpu_temp = round(
                temps["k10temp"][0].current
            )

        if "amdgpu" in temps and temps["amdgpu"]:
            gpu_temp = round(
                temps["amdgpu"][0].current
            )

        if "nvme" in temps and temps["nvme"]:
            nvme_temp = round(
                temps["nvme"][0].current
            )

    except Exception:
        pass

    return cpu_temp, gpu_temp, nvme_temp


def get_fan_speed():
    try:
        fans = psutil.sensors_fans()

        if "dell_smm" in fans and fans["dell_smm"]:
            return round(
                fans["dell_smm"][0].current
            )

    except Exception:
        pass

    return 0


def get_battery():
    try:
        battery = psutil.sensors_battery()

        if battery is None:
            return 0, 0

        return (
            round(battery.percent),
            1 if battery.power_plugged else 0
        )

    except Exception:
        return 0, 0


def get_uptime():
    try:
        return int(
            time.time() - psutil.boot_time()
        )

    except Exception:
        return 0


def bytes_per_second(current, previous, elapsed):
    if elapsed <= 0:
        return 0

    difference = max(
        0,
        current - previous
    )

    return int(
        difference / elapsed
    )


# ============================================================
# COMMAND HELPER
# ============================================================

def run_command(command, timeout=3):
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=timeout,
            check=False
        )

        return result.stdout.strip()

    except Exception:
        return ""


# ============================================================
# WAKATIME
# ============================================================

def get_wakatime_today_minutes():

    if not os.path.isfile(WAKATIME_CLI):
        return 0

    output = run_command(
        [WAKATIME_CLI, "--today"],
        timeout=10
    )

    if not output:
        return 0

    output = output.lower()

    hours = 0.0
    minutes = 0

    hour_match = re.search(
        r"(\d+(?:\.\d+)?)\s*(?:hrs?|hours?)",
        output
    )

    minute_match = re.search(
        r"(\d+)\s*(?:mins?|minutes?)",
        output
    )

    if hour_match:
        hours = float(
            hour_match.group(1)
        )

    if minute_match:
        minutes = int(
            minute_match.group(1)
        )

    # También soporta:
    # 5h 20m

    if not hour_match:
        hour_match = re.search(
            r"(\d+(?:\.\d+)?)\s*h",
            output
        )

        if hour_match:
            hours = float(
                hour_match.group(1)
            )

    if not minute_match:
        minute_match = re.search(
            r"(\d+)\s*m",
            output
        )

        if minute_match:
            minutes = int(
                minute_match.group(1)
            )

    return max(
        0,
        round(hours * 60 + minutes)
    )


# ============================================================
# PROJECT DETECTION
# ============================================================

JETBRAINS_NAMES = (
    "idea",
    "intellij",
    "webstorm",
    "goland",
    "pycharm",
    "phpstorm",
    "clion",
    "rubymine",
    "rider"
)


def get_git_root(path):

    if not path:
        return None

    path = path.strip().strip('"')

    if not os.path.exists(path):
        return None

    if os.path.isfile(path):
        path = os.path.dirname(path)

    root = run_command(
        [
            "git",
            "-C",
            path,
            "rev-parse",
            "--show-toplevel"
        ]
    )

    if root and os.path.isdir(root):
        return root

    return None


def discover_project_candidates():

    candidates = []

    home = os.path.expanduser("~")

    for proc in psutil.process_iter(
        ["pid", "name", "cmdline", "create_time"]
    ):

        try:
            cmdline = proc.info["cmdline"]

            if not cmdline:
                continue

            text = " ".join(cmdline)
            lower = text.lower()

            # ----------------------------------------
            # JetBrains
            # ----------------------------------------

            if any(
                name in lower
                for name in JETBRAINS_NAMES
            ):

                # IntelliJ / JPS
                marker = "-Dpreload.project.path="

                if marker in text:

                    start = text.find(marker)
                    start += len(marker)

                    end = text.find(
                        " -D",
                        start
                    )

                    if end == -1:
                        end = len(text)

                    path = text[start:end].strip()

                    candidates.append(
                        (
                            path,
                            100,
                            proc.info["create_time"]
                        )
                    )

                # WebStorm / TypeScript
                probe = "--pluginProbeLocations"

                if probe in cmdline:

                    try:
                        index = cmdline.index(probe)

                        value = cmdline[index + 1]

                        for path in value.split(","):

                            path = path.strip()

                            if path.startswith(home):

                                candidates.append(
                                    (
                                        path,
                                        95,
                                        proc.info["create_time"]
                                    )
                                )

                    except Exception:
                        pass

            # ----------------------------------------
            # Spring Boot / Java
            # ----------------------------------------

            if "/target/classes" in text:

                for token in cmdline:

                    if (
                        token.startswith(home)
                        and "/target/classes" in token
                    ):

                        path = token.split(
                            "/target/classes"
                        )[0]

                        candidates.append(
                            (
                                path,
                                90,
                                proc.info["create_time"]
                            )
                        )

            # ----------------------------------------
            # Node / Vite / TS
            # ----------------------------------------

            if (
                "node" in lower
                or "vite" in lower
                or "typescript" in lower
            ):

                for token in cmdline:

                    if token.startswith(home):

                        candidates.append(
                            (
                                token,
                                70,
                                proc.info["create_time"]
                            )
                        )

        except (
            psutil.NoSuchProcess,
            psutil.AccessDenied,
            KeyError
        ):
            pass

    return candidates


def newest_repo_file_time(root):

    newest = 0

    ignored = {
        ".git",
        ".idea",
        "node_modules",
        "target",
        "build",
        "dist",
        ".gradle"
    }

    try:

        for directory, dirs, files in os.walk(root):

            dirs[:] = [
                d for d in dirs
                if d not in ignored
            ]

            for filename in files:

                path = os.path.join(
                    directory,
                    filename
                )

                try:
                    mtime = os.path.getmtime(path)

                    if mtime > newest:
                        newest = mtime

                except OSError:
                    pass

    except Exception:
        pass

    return newest


def detect_active_repository():

    candidates = discover_project_candidates()

    repositories = {}

    now = time.time()

    for path, base_score, process_time in candidates:

        root = get_git_root(path)

        if not root:
            continue

        score = base_score

        newest = newest_repo_file_time(root)

        if newest:

            age = now - newest

            if age < 120:
                score += 50

            elif age < 600:
                score += 30

            elif age < 3600:
                score += 15

        # pequeño desempate:
        # proceso más reciente

        process_age = now - process_time

        if process_age < 3600:
            score += 5

        previous = repositories.get(root)

        if (
            previous is None
            or score > previous
        ):
            repositories[root] = score

    if not repositories:
        return None

    return max(
        repositories,
        key=repositories.get
    )


# ============================================================
# GIT
# ============================================================

def sanitize(value, maximum):

    value = str(value)

    value = value.replace(
        "|",
        "/"
    )

    value = value.replace(
        "\n",
        " "
    )

    value = value.replace(
        "\r",
        " "
    )

    # El protocolo USB será ASCII.
    value = value.encode(
        "ascii",
        errors="replace"
    ).decode("ascii")

    value = re.sub(
        r"\s+",
        "_",
        value
    )

    value = value.strip("_")

    if not value:
        value = "--"

    return value[:maximum]


def get_dev_info():

    root = detect_active_repository()

    if root is None:
        return "--", "--", 0

    project = os.path.basename(
        root.rstrip("/")
    )

    branch = run_command(
        [
            "git",
            "-C",
            root,
            "branch",
            "--show-current"
        ]
    )

    if not branch:

        branch = run_command(
            [
                "git",
                "-C",
                root,
                "rev-parse",
                "--short",
                "HEAD"
            ]
        )

    status = run_command(
        [
            "git",
            "-C",
            root,
            "status",
            "--porcelain"
        ]
    )

    modified = len(
        [
            line
            for line in status.splitlines()
            if line.strip()
        ]
    )

    return (
        sanitize(project, 20),
        sanitize(branch, 16),
        min(modified, 999)
    )


# ============================================================
# START
# ============================================================

print(
    "======================================"
)

print(
    "   LINUX -> ESP32 SYSTEM MONITOR V6"
)

print(
    "======================================"
)

print(
    f"\nPuerto : {PORT}"
    f"\nBaudios: {BAUD}\n"
)

print(
    "SYSTEM + GPU + STORAGE + NETWORK + DEV"
)

print(
    "Ctrl+C para salir\n"
)


try:

    ser = serial.Serial(
        PORT,
        BAUD,
        timeout=1
    )

except serial.SerialException as e:

    print(
        "ERROR abriendo el puerto:",
        e
    )

    raise SystemExit(1)


print(
    "Esperando al ESP32..."
)

time.sleep(2)

print(
    "ESP32 conectado.\n"
)


# ============================================================
# INITIAL STATE
# ============================================================

psutil.cpu_percent(
    interval=None
)

previous_net = psutil.net_io_counters()

previous_net_time = time.monotonic()


wakatime_minutes = 0

dev_project = "--"
dev_branch = "--"
dev_modified = 0

last_wakatime_refresh = 0
last_dev_refresh = 0


# ============================================================
# MAIN LOOP
# ============================================================

try:

    while True:

        now = time.monotonic()

        # --------------------------------------------
        # WAKATIME
        # --------------------------------------------

        if (
            last_wakatime_refresh == 0
            or
            now - last_wakatime_refresh
            >= WAKATIME_REFRESH
        ):

            wakatime_minutes = (
                get_wakatime_today_minutes()
            )

            last_wakatime_refresh = now


        # --------------------------------------------
        # DEV / GIT
        # --------------------------------------------

        if (
            last_dev_refresh == 0
            or
            now - last_dev_refresh
            >= DEV_REFRESH
        ):

            (
                dev_project,
                dev_branch,
                dev_modified
            ) = get_dev_info()

            last_dev_refresh = now


        # --------------------------------------------
        # SYSTEM METRICS
        # --------------------------------------------

        cpu = round(
            psutil.cpu_percent(
                interval=0.5
            )
        )

        ram = round(
            psutil.virtual_memory().percent
        )

        (
            cpu_temp,
            gpu_temp,
            nvme_temp
        ) = get_temperatures()

        fan = get_fan_speed()

        gpu = get_gpu_load()

        disk_percent = round(
            psutil.disk_usage("/").percent
        )

        battery, plugged = get_battery()

        uptime = get_uptime()


        # --------------------------------------------
        # NETWORK
        # --------------------------------------------

        current_net = (
            psutil.net_io_counters()
        )

        current_net_time = (
            time.monotonic()
        )

        elapsed = (
            current_net_time
            - previous_net_time
        )

        download_bps = bytes_per_second(
            current_net.bytes_recv,
            previous_net.bytes_recv,
            elapsed
        )

        upload_bps = bytes_per_second(
            current_net.bytes_sent,
            previous_net.bytes_sent,
            elapsed
        )

        previous_net = current_net

        previous_net_time = (
            current_net_time
        )

        download_bps = min(
            download_bps,
            999999999
        )

        upload_bps = min(
            upload_bps,
            999999999
        )


        # --------------------------------------------
        # SERIAL PACKET V6
        # --------------------------------------------

        message = (

            f"C{cpu}"
            f"|R{ram}"
            f"|T{cpu_temp}"
            f"|F{fan}"

            f"|G{gpu}"
            f"|GT{gpu_temp}"

            f"|D{disk_percent}"
            f"|N{nvme_temp}"

            f"|B{battery}"
            f"|P{plugged}"

            f"|U{uptime}"

            f"|RX{download_bps}"
            f"|TX{upload_bps}"

            f"|WT{wakatime_minutes}"

            f"|PJ{dev_project}"
            f"|BR{dev_branch}"
            f"|GM{dev_modified}"

            "\n"
        )


        try:

            ser.write(
                message.encode(
                    "ascii"
                )
            )

            ser.flush()

        except serial.SerialException:

            print(
                "\n\nESP32 desconectado."
            )

            break


        # --------------------------------------------
        # TERMINAL
        # --------------------------------------------

        hours = uptime // 3600

        minutes = (
            uptime % 3600
        ) // 60


        waka_hours = (
            wakatime_minutes // 60
        )

        waka_minutes_rest = (
            wakatime_minutes % 60
        )


        power_text = (
            "AC"
            if plugged
            else "BAT"
        )


        print(

            "\r"

            f"CPU {cpu:3d}%"

            f" | RAM {ram:3d}%"

            f" | TEMP {cpu_temp:2d}C"

            f" | FAN {fan:4d}"

            f" | GPU {gpu:3d}%"

            f" | GPU {gpu_temp:2d}C"

            f" | SSD {disk_percent:3d}%"

            f" | NVME {nvme_temp:2d}C"

            f" | BAT {battery:3d}% "
            f"{power_text}"

            f" | DOWN "
            f"{download_bps/1048576:6.2f} MB/s"

            f" | UP "
            f"{upload_bps/1048576:6.2f} MB/s"

            f" | DEV "
            f"{waka_hours}h"
            f"{waka_minutes_rest:02d}"

            f" {dev_project}"

            f" [{dev_branch}]"

            f" {dev_modified} MOD"

            f" | UPTIME "
            f"{hours}h"
            f"{minutes:02d}",

            end="",
            flush=True
        )


        time.sleep(0.5)


except KeyboardInterrupt:

    print(
        "\n\nMonitor detenido."
    )


finally:

    if ser.is_open:
        ser.close()

    print(
        "Puerto serial cerrado."
    )
