# PlatformIO extra script: choose the ST-Link to flash by the MCU's unique id (UID).
#
# Several ST-Links can be connected at the same time. Before "pio run -t upload" this
# script connects to every ST-Link (read only, no halt, no reset), reads the 96-bit UID of
# the MCU behind it and flashes only the one whose UID matches custom_mcu_uid in
# platformio.ini. The UID is burnt into the chip, so it follows the board and not the
# USB port or the programmer.
#
#   custom_mcu_uid = <24 hex digits>   flash only this board
#   custom_mcu_uid =                   (empty) use the ST-Link if exactly one is connected
#   MCU_UID=<uid> pio run -t upload    override once, without editing platformio.ini
#   pio run -t boards                  list connected ST-Links and their UIDs
#
# Linux only (the ST-Links are found in /sys/bus/usb/devices). On other systems a warning
# is printed and the upload runs as before, without the check.

import os
import subprocess
import sys

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

STLINK_VID = "0483"
STLINK_PIDS = {"3744", "3748", "374b", "374d", "374e", "374f", "3752", "3753", "3754", "3757"}

# Address of the 96-bit unique id, by STM32 family (build.mcu prefix).
UID_ADDRESS = {
    "stm32f0": 0x1FFFF7AC, "stm32f1": 0x1FFFF7E8, "stm32f2": 0x1FFF7A10,
    "stm32f3": 0x1FFFF7AC, "stm32f4": 0x1FFF7A10, "stm32f7": 0x1FF0F420,
    "stm32g0": 0x1FFF7590, "stm32g4": 0x1FFF7590, "stm32h7": 0x1FF1E800,
    "stm32l0": 0x1FF80050, "stm32l1": 0x1FF80050, "stm32l4": 0x1FFF7590,
}


def _say(msg=""):
    print(msg)
    sys.stdout.flush()


def _wanted_uid():
    uid = os.environ.get("MCU_UID")
    source = "MCU_UID"
    if uid is None:
        uid = env.GetProjectOption("custom_mcu_uid", "")  # noqa: F821
        source = "custom_mcu_uid"
    return uid.strip().lower().replace(" ", ""), source


def _uid_address():
    mcu = env.BoardConfig().get("build.mcu", "").lower()  # noqa: F821
    for prefix, address in UID_ADDRESS.items():
        if mcu.startswith(prefix):
            return address
    return None


def _openocd_base_args():
    # The upload flags without the final "program ..." command.
    flags = list(env["UPLOADERFLAGS"])  # noqa: F821
    for i, flag in enumerate(flags):
        if isinstance(flag, str) and flag.startswith("program "):
            return [env.subst(f) for f in flags[:i - 1]]  # noqa: F821
    return [env.subst(f) for f in flags]  # noqa: F821


def _find_stlinks():
    root = "/sys/bus/usb/devices"
    found = []
    for name in sorted(os.listdir(root)):
        path = os.path.join(root, name)
        try:
            with open(os.path.join(path, "idVendor")) as f:
                vid = f.read().strip()
            with open(os.path.join(path, "idProduct")) as f:
                pid = f.read().strip()
        except OSError:
            continue
        if vid == STLINK_VID and pid in STLINK_PIDS:
            found.append(name)
    return found


def _read_uid(location, address):
    openocd_dir = env.PioPlatform().get_package_dir("tool-openocd") or ""  # noqa: F821
    cmd = [os.path.join(openocd_dir, "bin", "openocd")] + _openocd_base_args()
    # adapter usb location must be given after the interface config, before init.
    cmd += ["-c", "adapter usb location %s" % location,
            "-c", "init",
            "-c", "echo [format UID=%%08x%%08x%%08x {*}[read_memory 0x%08X 32 3]]" % address,
            "-c", "shutdown"]
    try:
        res = subprocess.run(cmd, env=env["ENV"], capture_output=True, text=True,  # noqa: F821
                             timeout=20)
        text = res.stdout + res.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return None, str(e)
    for line in text.splitlines():
        if line.startswith("UID="):
            return line[4:].strip().lower(), None
    return None, "kunde inte läsas (upptagen, ej ansluten eller ingen spänning?)"


def _scan():
    address = _uid_address()
    if address is None:
        _say("select_stlink: okänd MCU-familj, UID-adress saknas.")
        env.Exit(1)  # noqa: F821
    boards = []
    for location in _find_stlinks():
        uid, err = _read_uid(location, address)
        boards.append((location, uid, err))
    return boards


def _print_boards(boards, wanted=""):
    if not boards:
        _say("  (inga ST-Link hittades)")
    for location, uid, err in boards:
        mark = "  <-- matchar" if wanted and uid == wanted else ""
        _say("  ST-Link %-12s UID %s%s" % (location, uid if uid else "? " + err, mark))


def select_board(source, target, env):
    if not sys.platform.startswith("linux"):
        _say("VARNING select_stlink: kortvalet via MCU UID fungerar bara på Linux. "
             "Flashar utan kontroll av vilket kort som är anslutet.")
        return

    wanted, origin = _wanted_uid()
    boards = _scan()
    readable = [b for b in boards if b[1]]
    _say("")
    _say("Val av kort via MCU UID (%s = %s):" % (origin, wanted if wanted else "tom"))
    _print_boards(boards, wanted)

    if wanted:
        matches = [b for b in readable if b[1] == wanted]
        if len(matches) != 1:
            _say("")
            if not matches:
                _say("AVBRYTER: inget anslutet kort har UID %s (%s)." % (wanted, origin))
            else:
                _say("AVBRYTER: flera ST-Link visar UID %s." % wanted)
            _say("Är det ett nytt kort? Byt custom_mcu_uid i platformio.ini till dess UID,")
            _say("eller flasha en gång med:  MCU_UID=<uid> pio run -t upload")
            env.Exit(1)
        chosen = matches[0]
    else:
        if len(boards) != 1:
            _say("")
            _say("AVBRYTER: custom_mcu_uid är tom och %d ST-Link är anslutna." % len(boards))
            _say("Sätt custom_mcu_uid i platformio.ini till UID:t för kortet som ska flashas.")
            env.Exit(1)
        chosen = boards[0]

    _say("Flashar via ST-Link %s (UID %s)" % (chosen[0], chosen[1] or "okänt"))
    _say("")

    flags = list(env["UPLOADERFLAGS"])
    for i, flag in enumerate(flags):
        if isinstance(flag, str) and flag.startswith("program "):
            flags[i - 1:i - 1] = ["-c", "adapter usb location %s" % chosen[0]]
            break
    env.Replace(UPLOADERFLAGS=flags)


def list_boards(source, target, env):
    wanted, origin = _wanted_uid()
    _say("")
    _say("Anslutna ST-Link (%s = %s):" % (origin, wanted if wanted else "tom"))
    _print_boards(_scan(), wanted)
    _say("")


if env.GetProjectOption("upload_protocol", "") == "stlink":  # noqa: F821
    env.AddPreAction("upload", select_board)  # noqa: F821
    env.AddCustomTarget(  # noqa: F821
        name="boards", dependencies=None, actions=[list_boards],
        title="List boards", description="Lista anslutna ST-Link och MCU UID")
