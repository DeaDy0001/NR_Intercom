#!/usr/bin/env python3
"""
NR Intercom - Werkzeug-Fenster: Flashen, Konsole und Befehle fuer C5 und S3 an einem Ort.

Start unter Windows: NR-Intercom.bat (Doppelklick). Direkt: py tools\\nrtool.py
Im Fenster 'help' eingeben. Braucht Python 3 mit esptool (bringt pyserial mit): py -m pip install esptool
"""
import json
import os
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
    "c5": {"chip": "esp32c5", "name": "nr-bodypack-c5", "boot": "0x2000", "title": "Bodypack ESP32-C5 (Funk/Audio)"},
    "s3": {"chip": "esp32s3", "name": "nr-display-s3", "boot": "0x0", "title": "Display ESP32-S3"},
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

    status           ALLES auf einen Blick: Firmware-Stand am PC, Ports, Zustand von C5 und S3
    c5 status        nur der C5 (WLAN, Audio, RTP, CPU, RAM, Intercom), ebenso: s3 status
    c5 <befehl>      einen Befehl am C5 ausfuehren und die Antwort zeigen, z. B.
                     c5 com    c5 help    c5 rxgain 18    c5 com find    (am S3: s3 help, s3 status)

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
    for addr, fn in ((b["boot"], f'{b["name"]}-bootloader.bin'), ("0x8000", f'{b["name"]}-partitions.bin'),
                     ("0x10000", f'{b["name"]}-app.bin')):
        path = os.path.join(BIN_DIR, fn)
        if not os.path.exists(path):
            warn(f"  Datei fehlt: {path}  -> zuerst 'pull'")
            return False
        args += [addr, path]
    major = int(str(getattr(esptool, "__version__", "5")).split(".")[0] or 5)
    cmd = "write-flash" if major >= 5 else "write_flash"
    for baud in (921600, 115200):
        info(f"  flashe {b['title']} auf {port} ({baud} Baud) ...")
        try:
            esptool.main(["--chip", b["chip"], "--port", port, "--baud", str(baud), cmd] + args)
            info(f"  {board.upper()} fertig geflasht, startet neu. Einstellungen bleiben erhalten.")
            return True
        except SystemExit as e:
            if e.code in (0, None):
                return True
        except Exception as e:
            warn(f"  Fehler: {e}")
    warn("  FLASHEN FEHLGESCHLAGEN. Arduino-Monitor zu? Sonst: BOOT halten, kurz RESET, BOOT loslassen, nochmal.")
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
    elif cmd == "update":
        pull_then("flash-all")
    elif cmd == "flash-all":            # intern: nach dem Neustart von 'update'
        flash("c5")
        flash("s3")
    elif cmd in BOARDS:
        board = cmd
        sub = parts[1].lower() if len(parts) > 1 else "help"
        if sub == "update":
            pull_then(f"{board} flash")
        elif sub == "flash":
            flash(board)
        elif sub == "monitor":
            monitor(board)
        elif sub == "reset":
            reset(board)
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
