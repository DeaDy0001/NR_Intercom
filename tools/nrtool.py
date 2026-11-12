#!/usr/bin/env python3
"""
NR Intercom - Werkzeug-Fenster: Flashen, Konsole und Befehle fuer C5 und S3 an einem Ort.

Start unter Windows: NR-Intercom.bat (Doppelklick). Direkt: py tools\\nrtool.py
Im Fenster 'help' eingeben. Braucht Python 3 mit esptool (bringt pyserial mit): py -m pip install esptool
"""
import json
import os
import socket
import struct
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import shlex
import subprocess
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial fehlt. Einmalig: py -m pip install esptool")
    sys.exit(2)

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, ".."))
BIN_DIR = os.path.join(REPO, "firmware", "bin")
PORTS_FILE = os.path.join(HERE, ".nrtool-ports.json")
IS_WIN = os.name == "nt"
SELF = os.path.abspath(__file__)


def file_hash(path):
    try:
        with open(path, "rb") as f:
            return hash(f.read())
    except OSError:
        return None


SELF_HASH = file_hash(SELF)     # Stand beim Start: aendert git pull das Werkzeug, startet es sich neu

BOARDS = {
    # force: Register + Maske der Flash-Modus-Anforderung ('download'), die einen Reset ueberlebt
    "c5": {"chip": "esp32c5", "name": "nr-bodypack-c5", "boot": "0x2000", "title": "Bodypack ESP32-C5 (Funk/Audio)",
           "force": ("0x600B1034", "0x60000000")},     # LP_AON_SYS_CFG_REG, LP_AON_FORCE_DOWNLOAD_BOOT
    "s3": {"chip": "esp32s3", "name": "nr-display-s3", "boot": "0x0", "title": "Display ESP32-S3",
           "force": ("0x6000812C", "0x1")},            # RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT
}
CTRL_Q, CTRL_C, CTRL_R = "\x11", "\x03", "\x12"

HELP = """
  Befehle (c5 = Bodypack/Funk-Audio, s3 = Display):

    c5 update        Firmware holen (git pull) und den C5 flashen
    s3 update        dasselbe fuer den S3
    update           git pull, dann C5 und S3 flashen
    c5 flash         nur flashen (ohne git pull), ebenso: s3 flash
    pull             nur die neueste Firmware holen (git pull)

    c5 monitor       Konsole des C5 (Log mitlesen, Befehle tippen). Zurueck mit Strg+Q
    s3 monitor       Log des S3. Zurueck mit Strg+Q
    c5 reset         C5 neu starten, ebenso: s3 reset
    c5 rescue        Board bleibt nach dem Flashen dunkel (haengt im Flash-Modus)? Das holt es raus.
                     Alternativ: USB-Kabel 3 s abziehen und wieder anstecken.

    status           ALLES auf einen Blick: Firmware-Stand am PC, Ports, Zustand von C5 und S3
    c5 status        nur der C5 (WLAN, Audio, RTP, CPU, RAM, Intercom), ebenso: s3 status
    c5 <befehl>      einen Befehl am C5 ausfuehren und die Antwort zeigen, z. B.
                     c5 com    c5 help    c5 rxgain 18    c5 com find    (am S3: s3 help, s3 status)

    Ueber WLAN (ohne USB-Kabel, Bodypack muss im WLAN sein):
    devices          Bodypacks im Netzwerk suchen (Name, IP, Firmware C5/S3)
    c5 wlan          Firmware holen (git pull) und den C5 ueber WLAN aktualisieren
    s3 wlan          dasselbe fuer den S3 (der S3 braucht ein 2,4-GHz-WLAN)
    wlan             git pull, dann S3 und C5 ueber WLAN
    code <code>      Update-Code festlegen (Standard nrintercom, am C5: otacode <code>)
    Einmalig vorher per USB: 'update' (neue Speicheraufteilung mit zwei Programmbereichen).
    Windows-Firewall: Python beim ersten Mal fuer "Private Netzwerke" erlauben.

    ports            angeschlossene COM-Ports anzeigen
    c5 port COM6     COM-Port fuer den C5 festlegen, ebenso: s3 port COM5
    help             diese Hilfe
    exit             beenden

  Beim Flashen bleiben WLAN, PIN und alle Einstellungen erhalten.
  Klappt das Flashen nicht: BOOT halten, kurz RESET druecken, BOOT loslassen, nochmal versuchen.
  Der Monitor der Arduino IDE muss geschlossen sein (er blockiert den Port).
"""


def info(msg):
    sys.stdout.write("\x1b[36m" + msg + "\x1b[0m\n")
    sys.stdout.flush()


def warn(msg):
    sys.stdout.write("\x1b[33m" + msg + "\x1b[0m\n")
    sys.stdout.flush()


# ---------------------------------------------------------------- COM-Ports
def load_ports():
    try:
        with open(PORTS_FILE, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_port(board, port):
    p = load_ports()
    p[board] = port
    try:
        with open(PORTS_FILE, "w", encoding="utf-8") as f:
            json.dump(p, f)
    except OSError:
        pass


def norm_port(p):
    p = p.strip()
    return "COM" + p if IS_WIN and p.isdigit() else p


def show_ports():
    saved = load_ports()
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    if not ports:
        warn("  keine seriellen Geraete gefunden (USB-Kabel? Board eingesteckt?)")
    for p in ports:
        who = [b.upper() for b, d in saved.items() if d == p.device]
        mark = f"   <- {', '.join(who)}" if who else ""
        print(f"  {p.device:8s} {p.description}{mark}")


def port_for(board, ask=True):
    port = load_ports().get(board)
    present = [p.device for p in list_ports.comports()]
    if port and port in present:
        return port
    if not ask:
        return None
    if port:
        warn(f"  {board.upper()}: gespeicherter Port {port} ist gerade nicht da.")
    show_ports()
    ans = input(f"  COM-Port fuer {board.upper()} (z. B. COM6 oder 6, Enter = abbrechen): ").strip()
    if not ans:
        return None
    port = norm_port(ans)
    save_port(board, port)
    return port


# ---------------------------------------------------------------- git pull
def git_pull():
    try:
        branch = subprocess.run(["git", "-C", REPO, "rev-parse", "--abbrev-ref", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
    except FileNotFoundError:
        warn("  git nicht gefunden - Firmware bitte von Hand aktualisieren (oder 'c5 flash').")
        return False
    if not branch or branch == "HEAD":
        warn("  kein git-Branch erkannt - ueberspringe git pull.")
        return False
    info(f"  hole neueste Firmware (git pull origin {branch}) ...")
    r = subprocess.run(["git", "-C", REPO, "pull", "--ff-only", "origin", branch])
    if r.returncode != 0:
        warn("  git pull hat nicht geklappt (lokale Aenderungen?). Es wird der vorhandene Stand geflasht.")
        return False
    return True


def pull_then(next_cmd):
    """git pull; hat sich dabei dieses Werkzeug geaendert, mit der neuen Version weitermachen."""
    git_pull()
    if file_hash(SELF) != SELF_HASH:
        info("  Das Werkzeug selbst wurde aktualisiert - starte die neue Version ...")
        rc = subprocess.call([sys.executable, SELF, "--continue"] + ([next_cmd] if next_cmd else []))
        sys.exit(rc)
    if next_cmd:
        handle(next_cmd)


# ---------------------------------------------------------------- Flashen
def rescue(board):
    """Board haengt im Flash-Modus (LED aus, keine Verbindung): Anforderung loeschen und normal starten."""
    try:
        import esptool
    except ImportError:
        warn("  esptool fehlt. Einmalig: py -m pip install esptool")
        return
    port = port_for(board)
    if not port:
        return
    b = BOARDS[board]
    major = int(str(getattr(esptool, "__version__", "5")).split(".")[0] or 5)
    info(f"  {board.upper()}: loesche die Flash-Modus-Anforderung und starte normal ...")
    try:
        esptool.main(["--chip", b["chip"], "--port", port, "--before", "no-reset" if major >= 5 else "no_reset",
                      "write-mem" if major >= 5 else "write_mem", b["force"][0], "0", b["force"][1]])
        info("  erledigt. Startet es trotzdem nicht: USB-Kabel 3 s abziehen und wieder anstecken.")
    except (SystemExit, Exception) as e:
        warn(f"  hat nicht geklappt ({e}). Dann: USB-Kabel 3 s abziehen und wieder anstecken.")


def flash(board):
    try:
        import esptool
    except ImportError:
        warn("  esptool fehlt. Einmalig: py -m pip install esptool")
        return False
    port = port_for(board)
    if not port:
        return False
    b = BOARDS[board]
    args = []
    # Bootloader, Partitionstabelle, Startinfo (immer Bereich ota_0) und Programm; 0x9000-0xEFFF (Einstellungen) bleibt
    for addr, fn in ((b["boot"], f'{b["name"]}-bootloader.bin'), ("0x8000", f'{b["name"]}-partitions.bin'),
                     ("0xf000", f'{b["name"]}-otadata.bin'), ("0x20000", f'{b["name"]}-app.bin')):
        path = os.path.join(BIN_DIR, fn)
        if not os.path.exists(path):
            warn(f"  Datei fehlt: {path}  -> zuerst 'pull'")
            return False
        args += [addr, path]
    major = int(str(getattr(esptool, "__version__", "5")).split(".")[0] or 5)
    cmd = "write-flash" if major >= 5 else "write_flash"
    no_reset = "no-reset" if major >= 5 else "no_reset"

    def run(baud, before=None, after=None, command=None):
        argv = ["--chip", b["chip"], "--port", port, "--baud", str(baud)]
        if before:
            argv += ["--before", before]
        if after:
            argv += ["--after", after]
        try:
            esptool.main(argv + (command or [cmd] + args))
            return True
        except SystemExit as e:
            return e.code in (0, None)
        except Exception as e:
            warn(f"  Fehler: {e}")
            return False

    # 1) normal: esptool setzt das Board ueber USB selbst in den Flash-Modus
    info(f"  flashe {b['title']} auf {port} ...")
    ok = run(921600)
    # 2) sonst per Befehl: die Firmware startet selbst in den Flash-Modus (ohne BOOT-Taste)
    if not ok:
        info("  automatischer Reset hat nicht geklappt - schalte das Board per Befehl in den Flash-Modus ...")
        try:
            s = open_serial(port)
            time.sleep(0.2)
            s.write(b"\ndownload\n")
            time.sleep(0.3)
            s.close()
        except Exception as e:
            warn(f"  konnte den Befehl nicht senden: {e}")
        # Board startet neu, der USB-Port verschwindet kurz und kommt wieder
        t_end = time.time() + 8
        time.sleep(1.5)
        while time.time() < t_end and port not in [p.device for p in list_ports.comports()]:
            time.sleep(0.3)
        time.sleep(0.5)
        ok = run(921600, no_reset, no_reset) or run(115200, no_reset, no_reset)
        # Flash-Modus-Anforderung loeschen, dann normal neu starten (sonst startet das Board wieder im Flash-Modus)
        if ok:
            run(115200, no_reset, None, ["write-mem" if major >= 5 else "write_mem", b["force"][0], "0", b["force"][1]])
    if ok:
        info(f"  {board.upper()} fertig geflasht, startet neu. Einstellungen bleiben erhalten.")
        return True
    warn("  FLASHEN FEHLGESCHLAGEN. Monitor der Arduino IDE / ein zweites Fenster offen?")
    warn("  Notfalls von Hand: BOOT halten, kurz RESET druecken, BOOT loslassen, dann nochmal 'flash'.")
    return False


# ---------------------------------------------------------------- serielle Verbindung
def open_serial(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.05
    s.dtr = False          # beim Oeffnen KEINEN Reset / Download-Modus ausloesen
    s.rts = False
    s.open()
    return s


def clean(data):
    # Die ESP-Konsole fragt mit ESC[5n, ob das Terminal Steuerzeichen kann. Wir antworten bewusst nicht:
    # So bleibt die Konsole im einfachen Modus und Befehle mit Zeilenende \n funktionieren immer.
    return data.replace(b"\x1b[5n", b"").decode("utf-8", errors="replace")


def reset(board):
    port = port_for(board)
    if not port:
        return
    try:
        s = open_serial(port)
        s.rts = True
        time.sleep(0.1)
        s.rts = False
        s.close()
        info(f"  {board.upper()} startet neu.")
    except Exception as e:
        warn(f"  {port} nicht erreichbar: {e}")


def run_command(board, command, seconds, ask=True):
    """Befehl an die Konsole von C5 oder S3 schicken, Antwort anzeigen."""
    port = port_for(board, ask)
    if not port:
        warn(f"  {board.upper()}: kein Port (eingesteckt? '{board} port COMx' festlegen)")
        return
    try:
        s = open_serial(port)
    except Exception as e:
        warn(f"  {port} nicht erreichbar ({e}). Laeuft noch der Arduino-Monitor oder ein anderes Fenster?")
        return
    try:
        time.sleep(0.2)
        s.reset_input_buffer()
        s.write(command.encode("utf-8") + b"\n")
        end = time.time() + seconds
        quiet_since = None
        while time.time() < end:
            data = s.read(1024)
            if data:
                sys.stdout.write(clean(data))
                sys.stdout.flush()
                quiet_since = None
            else:
                quiet_since = quiet_since or time.time()
                if time.time() - quiet_since > 0.8 and time.time() > end - seconds + 1.0:
                    break          # Antwort ist vorbei
        print()
    finally:
        s.close()


def monitor(board):
    port = port_for(board)
    if not port:
        return
    stop = threading.Event()
    state = {"ser": None}
    lock = threading.Lock()
    info(f"  Monitor {board.upper()} auf {port}. Zurueck zu nr> mit Strg+Q, Neustart mit Strg+R.")

    def reader():
        waiting = False
        while not stop.is_set():
            if state["ser"] is None:
                try:
                    with lock:
                        state["ser"] = open_serial(port)
                    if waiting:
                        info(f"\n  wieder verbunden mit {port}")
                    waiting = False
                except Exception:
                    if not waiting:
                        info(f"\n  warte auf {port} ...")
                        waiting = True
                    time.sleep(0.5)
                    continue
            try:
                data = state["ser"].read(512)
            except Exception:
                with lock:
                    try:
                        state["ser"].close()
                    except Exception:
                        pass
                    state["ser"] = None
                continue
            if data:
                sys.stdout.write(clean(data))
                sys.stdout.flush()

    def send(b):
        with lock:
            if state["ser"]:
                try:
                    state["ser"].write(b)
                except Exception:
                    pass

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    try:
        if IS_WIN:
            import msvcrt
            while True:
                ch = msvcrt.getwch()
                if ch in (CTRL_Q, CTRL_C):
                    break
                if ch == CTRL_R:
                    with lock:
                        if state["ser"]:
                            state["ser"].rts = True
                            time.sleep(0.1)
                            state["ser"].rts = False
                    continue
                if ch in ("\x00", "\xe0"):          # Pfeiltasten usw. ignorieren
                    msvcrt.getwch()
                    continue
                if ch == "\r":
                    send(b"\n")
                elif ch == "\x08":
                    send(b"\x08")
                else:
                    send(ch.encode("utf-8"))
        else:
            import termios
            import tty
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                while True:
                    ch = os.read(fd, 16).decode("utf-8", errors="ignore")
                    if CTRL_Q in ch or CTRL_C in ch:
                        break
                    send(ch.replace("\r", "\n").encode("utf-8"))
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)
    except KeyboardInterrupt:
        pass
    stop.set()
    t.join(1)
    with lock:
        if state["ser"]:
            state["ser"].close()
    print()
    info("  Monitor beendet.")


# ---------------------------------------------------------------- Update ueber WLAN
OTA_PORT = 3011
HTTP_PORT = 8070
_http = None


def get_code():
    return load_ports().get("_code", "nrintercom")


def bin_version(path):
    """Versionstext aus dem ESP-App-Abbild (esp_app_desc_t ab Byte 32: Magic, ..., version[32] ab Byte 48)."""
    try:
        with open(path, "rb") as f:
            d = f.read(96)
        if struct.unpack("<I", d[32:36])[0] != 0xABCD5432:
            return "?"
        return d[48:80].split(b"\0")[0].decode("ascii", "replace")
    except OSError:
        return "?"


def discover(timeout=1.5):
    """Bodypacks per UDP-Broadcast suchen."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(0.2)
    found = {}
    end = time.time() + timeout
    next_send = 0
    while time.time() < end:
        if time.time() >= next_send:
            next_send = time.time() + 0.5
            for addr in ("255.255.255.255",):
                try:
                    s.sendto(b"NRBP?", (addr, OTA_PORT))
                except OSError:
                    pass
        try:
            data, (ip, _) = s.recvfrom(1024)
            d = json.loads(data.decode("utf-8", "replace"))
            if d.get("nrbp"):
                d["ip"] = d.get("ip") or ip
                found[d.get("mac", ip)] = d
        except (socket.timeout, ValueError):
            pass
    s.close()
    return sorted(found.values(), key=lambda d: d.get("name", ""))


def show_devices(devs=None):
    devs = discover() if devs is None else devs
    if not devs:
        warn("  kein Bodypack im Netzwerk gefunden. Im selben WLAN/LAN? Firmware alt? -> einmalig per USB 'update'.")
        return devs
    for i, d in enumerate(devs, 1):
        print(f"  {i}. {d.get('name', '?'):16s} {d.get('ip', '?'):15s} C5 {d.get('c5', '?'):22s} "
              f"S3 {d.get('s3') or '?':22s} {d.get('user', '')}")
    return devs


def start_http():
    global _http
    if _http:
        return
    class H(SimpleHTTPRequestHandler):
        def __init__(self, *a, **k):
            super().__init__(*a, directory=BIN_DIR, **k)
        def log_message(self, *a):
            pass
    _http = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), H)
    threading.Thread(target=_http.serve_forever, daemon=True).start()


def pc_ip_towards(ip):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((ip, OTA_PORT))
        return s.getsockname()[0]
    finally:
        s.close()


def pick_devices():
    devs = show_devices()
    if len(devs) <= 1:
        return devs
    ans = input("  Welche? Nummer, mehrere mit Komma, 'alle' (Enter = abbrechen): ").strip().lower()
    if ans in ("alle", "all", "a"):
        return devs
    out = []
    for part in ans.replace(" ", "").split(","):
        if part.isdigit() and 1 <= int(part) <= len(devs):
            out.append(devs[int(part) - 1])
    return out


def ota_one(dev, board):
    b = BOARDS[board]
    fn = f'{b["name"]}-app.bin'
    path = os.path.join(BIN_DIR, fn)
    if not os.path.exists(path):
        warn(f"  Datei fehlt: {path}  -> zuerst 'pull'")
        return False
    ip = dev["ip"]
    try:
        start_http()
    except OSError as e:
        warn(f"  Datei-Server auf Port {HTTP_PORT} startet nicht: {e}")
        return False
    url = f"http://{pc_ip_towards(ip)}:{HTTP_PORT}/{fn}"
    info(f"  {dev.get('name')} ({ip}): {board.upper()} {dev.get(board) or '?'} -> {bin_version(path)}")
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    s.sendto(f"NRBP_OTA {get_code()} {board} {url}".encode(), (ip, OTA_PORT))
    end = time.time() + (180 if board == "s3" else 120)
    last = -1
    ok = False
    while time.time() < end:
        try:
            data, _ = s.recvfrom(1024)
            d = json.loads(data.decode("utf-8", "replace"))
        except (socket.timeout, ValueError):
            continue
        st = d.get("ota")
        if st == "start":
            print("  Update gestartet ...")
        elif st == "progress":
            pct = int(d.get("pct", 0))
            if pct != last:
                last = pct
                bar = "#" * (pct // 4)
                sys.stdout.write(f"\r  [{bar:25s}] {pct:3d} %")
                sys.stdout.flush()
            end = max(end, time.time() + 60)
        elif st == "done":
            print(f"\r  [{'#' * 25}] 100 %")
            ok = True
            break
        elif st == "error":
            print()
            warn(f"  Fehler: {d.get('msg', '?')}")
            break
    s.close()
    if not ok:
        if last < 0:
            warn("  keine Antwort vom Bodypack (Firewall am PC? Update-Code? Firmware alt -> einmalig per USB 'update')")
        return False
    info(f"  {board.upper()} startet neu, warte ...")
    t_done = time.time()
    for _ in range(40):
        time.sleep(1.5)
        for d in discover(1.0):
            if d.get("mac") == dev.get("mac") and (board == "c5" or d.get("s3")):
                if board == "s3" and d.get("s3") == dev.get("s3") and time.time() - t_done < 15:
                    continue          # S3 meldet die neue Version erst einige Sekunden nach seinem Neustart
                info(f"  {d.get('name')}: C5 {d.get('c5')}, S3 {d.get('s3') or '?'}  - fertig.")
                return True
    warn("  Bodypack meldet sich nicht zurueck. 'devices' oder 'c5 status' pruefen.")
    return False


def ota(boards):
    devs = pick_devices()
    for dev in devs:
        for board in boards:
            if not ota_one(dev, board):
                break
            dev = next((d for d in discover(1.0) if d.get("mac") == dev.get("mac")), dev)


# ---------------------------------------------------------------- Gesamtstatus
def overview():
    info("  ===== PC / Firmware-Dateien =====")
    try:
        g = lambda *a: subprocess.run(["git", "-C", REPO] + list(a), capture_output=True, text=True).stdout.strip()
        print(f"  Repository: Branch {g('rev-parse', '--abbrev-ref', 'HEAD')}, Stand {g('log', '-1', '--format=%h %cd %s', '--date=format:%d.%m. %H:%M')}")
    except FileNotFoundError:
        print("  Repository: git nicht gefunden")
    for b in BOARDS.values():
        p = os.path.join(BIN_DIR, b["name"] + "-app.bin")
        if os.path.exists(p):
            print(f"  {b['name']}-app.bin: {time.strftime('%d.%m. %H:%M', time.localtime(os.path.getmtime(p)))}, "
                  f"{os.path.getsize(p) // 1024} KB")
    info("  ===== Ports =====")
    show_ports()
    for board in ("c5", "s3"):
        info(f"  ===== {BOARDS[board]['title']} =====")
        run_command(board, "status", 3, ask=False)


# ---------------------------------------------------------------- Befehlszeile
def handle(line):
    try:
        parts = shlex.split(line, posix=not IS_WIN)
    except ValueError:
        parts = line.split()
    if not parts:
        return True
    cmd = parts[0].lower()
    if cmd in ("exit", "quit", "ende"):
        return False
    if cmd in ("help", "hilfe", "?"):
        print(HELP)
    elif cmd == "ports":
        show_ports()
    elif cmd == "status":
        overview()
    elif cmd == "pull":
        pull_then(None)
    elif cmd in ("devices", "geraete", "bodypacks"):
        show_devices()
    elif cmd == "wlan" or (cmd == "update" and len(parts) > 1 and parts[1].lower() == "wlan"):
        pull_then("ota-all")
    elif cmd == "ota-all":              # intern: S3 zuerst (der C5 reicht dessen Fortschritt weiter), dann C5
        ota(["s3", "c5"])
    elif cmd == "code":
        if len(parts) < 2:
            print(f"  Update-Code: {get_code()}")
        else:
            p = load_ports()
            p["_code"] = parts[1]
            try:
                with open(PORTS_FILE, "w", encoding="utf-8") as f:
                    json.dump(p, f)
            except OSError:
                pass
            info(f"  Update-Code gespeichert. Am C5 muss derselbe stehen: c5 otacode {parts[1]}")
    elif cmd == "update":
        pull_then("flash-all")
    elif cmd == "flash-all":            # intern: nach dem Neustart von 'update'
        flash("c5")
        flash("s3")
    elif cmd in BOARDS:
        board = cmd
        sub = parts[1].lower() if len(parts) > 1 else "help"
        if sub == "update" and len(parts) > 2 and parts[2].lower() == "wlan":
            pull_then(f"{board} ota")
        elif sub == "update":
            pull_then(f"{board} flash")
        elif sub == "wlan":
            pull_then(f"{board} ota")
        elif sub == "ota":              # intern: nach pull
            ota([board])
        elif sub == "flash":
            flash(board)
        elif sub == "monitor":
            monitor(board)
        elif sub == "reset":
            reset(board)
        elif sub == "rescue":
            rescue(board)
        elif sub == "port":
            if len(parts) < 3:
                print(f"  {board.upper()}: {load_ports().get(board, 'noch nicht festgelegt')}")
            else:
                save_port(board, norm_port(parts[2]))
                info(f"  {board.upper()} benutzt jetzt {norm_port(parts[2])}")
        else:
            rest = " ".join(parts[1:])
            seconds = 6 if parts[1].lower() in ("scan", "nettest", "pegel", "i2ctest", "i2cdiag") else 2.5
            run_command(board, rest, seconds)
    else:
        warn(f"  unbekannt: {line}   ('help' zeigt alle Befehle)")
    return True


def main():
    if IS_WIN:
        os.system("")      # Farben im Fenster aktivieren
    args = sys.argv[1:]
    if args and args[0] == "--continue":   # Neustart nach eigenem Update: Befehl fortsetzen, dann normal weiter
        print("\n  NR Intercom - Werkzeug (neue Version)   ('help' zeigt alle Befehle)\n")
        if len(args) > 1:
            handle(" ".join(args[1:]))
    elif args:             # auch direkt aufrufbar, z. B.: nrtool.py c5 update
        handle(" ".join(args))
        return
    else:
        print("\n  NR Intercom - Werkzeug   ('help' zeigt alle Befehle)\n")
    while True:
        try:
            line = input("nr> ")
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not handle(line):
            break


if __name__ == "__main__":
    main()
