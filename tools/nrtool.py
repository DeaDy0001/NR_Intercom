#!/usr/bin/env python3
"""
NR Intercom - Flashen und Konsole in einem Fenster (ersetzt den Arduino-IDE-Monitor)

    py tools\\nrtool.py c5          Bodypack (ESP32-C5): flashen, dann Konsole
    py tools\\nrtool.py s3          Display (ESP32-S3)
    py tools\\nrtool.py c5 --monitor   nur Konsole, ohne zu flashen

In der Konsole:
    Strg+F  neu flashen (Port wird dafuer kurz freigegeben, danach geht die Konsole weiter)
    Strg+R  Neustart des Boards
    Strg+Q  beenden (oder Strg+C)
Der Port wird automatisch wieder geoeffnet, wenn das Board neu startet oder kurz abgesteckt wird.
Gespeicherte Einstellungen (WLAN, PIN ...) bleiben beim Flashen erhalten (0x9000-0xFFFF wird nicht geschrieben).

Braucht: Python 3 mit esptool und pyserial  (py -m pip install esptool)
"""
import argparse
import json
import os
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial fehlt: py -m pip install esptool   (bringt pyserial mit)")
    sys.exit(2)

HERE = os.path.dirname(os.path.abspath(__file__))
BIN_DIR = os.path.normpath(os.path.join(HERE, "..", "firmware", "bin"))
PORTS_FILE = os.path.join(HERE, ".nrtool-ports.json")
IS_WIN = os.name == "nt"

BOARDS = {
    "c5": {"chip": "esp32c5", "name": "nr-bodypack-c5", "boot": "0x2000", "title": "Bodypack ESP32-C5 (Funk/Audio)"},
    "s3": {"chip": "esp32s3", "name": "nr-display-s3", "boot": "0x0", "title": "Display ESP32-S3"},
}

CTRL_F, CTRL_R, CTRL_Q, CTRL_C = "\x06", "\x12", "\x11", "\x03"


def say(msg):
    sys.stdout.write("\r\n\x1b[36m[nrtool] " + msg + "\x1b[0m\r\n")
    sys.stdout.flush()


# ---------------------------------------------------------------- Port waehlen
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


def choose_port(board, given):
    if given:
        return given
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    last = load_ports().get(board)
    print("Gefundene serielle Geraete:")
    for p in ports:
        mark = "  <- zuletzt" if p.device == last else ""
        print(f"    {p.device:8s} {p.description}{mark}")
    if not ports:
        print("    keine gefunden (USB-Kabel? Board eingesteckt?)")
    default = last if any(p.device == last for p in ports) else (ports[0].device if len(ports) == 1 else None)
    while True:
        prompt = f"COM-Port fuer {board.upper()} [Enter = {default}]: " if default else f"COM-Port fuer {board.upper()} (z. B. COM6 oder 6): "
        ans = input(prompt).strip()
        if not ans and default:
            ans = default
        if not ans:
            continue
        if IS_WIN and ans.isdigit():
            ans = "COM" + ans
        save_port(board, ans)
        return ans


# ---------------------------------------------------------------- Flashen
def flash(board, port):
    try:
        import esptool
    except ImportError:
        say("esptool fehlt: py -m pip install esptool")
        return False
    b = BOARDS[board]
    files = [(b["boot"], f'{b["name"]}-bootloader.bin'), ("0x8000", f'{b["name"]}-partitions.bin'),
             ("0x10000", f'{b["name"]}-app.bin')]
    args = []
    for addr, fn in files:
        path = os.path.join(BIN_DIR, fn)
        if not os.path.exists(path):
            say(f"Datei fehlt: {path}  (Repository aktualisieren: git pull)")
            return False
        args += [addr, path]
    for baud in (921600, 115200):
        say(f"Flashe {b['title']} auf {port} mit {baud} Baud ...")
        try:
            major = int(str(getattr(esptool, "__version__", "5")).split(".")[0] or 5)
            cmd = "write-flash" if major >= 5 else "write_flash"     # esptool 4 kennt nur die alte Schreibweise
            esptool.main(["--chip", b["chip"], "--port", port, "--baud", str(baud), cmd] + args)
            say("Flashen fertig, Board startet neu. Einstellungen bleiben erhalten.")
            return True
        except SystemExit as e:
            if e.code in (0, None):
                return True
        except Exception as e:  # esptool.FatalError, serial.SerialException ...
            say(f"Fehler: {e}")
    say("FLASHEN FEHLGESCHLAGEN. Download-Modus: BOOT halten, kurz RESET, BOOT loslassen, dann Strg+F.")
    return False


# ---------------------------------------------------------------- Konsole
class Monitor:
    def __init__(self, port):
        self.port = port
        self.ser = None
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.paused = threading.Event()    # waehrend des Flashens Port freigeben
        self.full_mode = False             # Konsole des Boards mit Zeileneditor (nach Terminal-Abfrage)
        self.t = threading.Thread(target=self.reader, daemon=True)

    def open(self):
        s = serial.Serial()
        s.port = self.port
        s.baudrate = 115200
        s.timeout = 0.05
        s.dtr = False                       # beim Oeffnen KEIN Reset/Download-Modus auslosen
        s.rts = False
        s.open()
        self.ser = s
        self.full_mode = False

    def close(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except Exception:
                    pass
                self.ser = None

    def reader(self):
        waiting_msg = False
        tail = b""
        while not self.stop.is_set():
            if self.paused.is_set():
                time.sleep(0.05)
                continue
            if self.ser is None:
                try:
                    self.open()
                    say(f"verbunden mit {self.port}  (Strg+F flashen, Strg+R Neustart, Strg+Q beenden)")
                    waiting_msg = False
                except Exception:
                    if not waiting_msg:
                        say(f"warte auf {self.port} ...")
                        waiting_msg = True
                    time.sleep(0.5)
                    continue
            try:
                data = self.ser.read(512)
            except Exception:
                self.close()
                say("Verbindung getrennt (Neustart?)")
                continue
            if not data:
                continue
            # Die Konsole des Boards fragt beim Start, ob das Terminal Steuerzeichen kann (ESC [5n).
            # Antwort ESC [0n -> Zeileneditor mit Pfeiltasten und Verlauf ist aktiv.
            probe = tail + data
            if b"\x1b[5n" in probe:
                try:
                    self.ser.write(b"\x1b[0n")
                    self.full_mode = True
                except Exception:
                    pass
                data = data.replace(b"\x1b[5n", b"")
            tail = probe[-3:]
            sys.stdout.write(data.decode("utf-8", errors="replace"))
            sys.stdout.flush()

    def send(self, b):
        with self.lock:
            if self.ser:
                try:
                    self.ser.write(b)
                except Exception:
                    pass

    def reset(self):
        with self.lock:
            if not self.ser:
                return
            try:                                # wie esptool: kurzer Reset ueber RTS
                self.ser.dtr = False
                self.ser.rts = True
                time.sleep(0.1)
                self.ser.rts = False
            except Exception:
                pass
        say("Neustart ausgeloest")


def read_keys_windows(mon, board):
    import msvcrt
    special = {"H": b"\x1b[A", "P": b"\x1b[B", "K": b"\x1b[D", "M": b"\x1b[C",
               "G": b"\x1b[H", "O": b"\x1b[F", "S": b"\x1b[3~"}
    while True:
        ch = msvcrt.getwch()
        if ch in (CTRL_Q, CTRL_C):
            return
        if ch == CTRL_F:
            do_flash(mon, board)
            continue
        if ch == CTRL_R:
            mon.reset()
            continue
        if ch in ("\x00", "\xe0"):              # Pfeil- und Sondertasten
            k = msvcrt.getwch()
            if k in special:
                mon.send(special[k])
            continue
        if ch == "\r":
            mon.send(b"\r" if mon.full_mode else b"\n")
        elif ch == "\x08":
            mon.send(b"\x7f" if mon.full_mode else b"\x08")
        else:
            mon.send(ch.encode("utf-8"))


def read_keys_posix(mon, board):
    import termios
    import tty
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        while True:
            ch = os.read(fd, 16).decode("utf-8", errors="ignore")
            if not ch:
                continue
            if CTRL_Q in ch or CTRL_C in ch:
                return
            if ch == CTRL_F:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)
                do_flash(mon, board)
                tty.setraw(fd)
                continue
            if ch == CTRL_R:
                mon.reset()
                continue
            if ch == "\r" and not mon.full_mode:
                ch = "\n"
            mon.send(ch.encode("utf-8"))
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def do_flash(mon, board):
    mon.paused.set()
    time.sleep(0.1)
    mon.close()
    flash(board, mon.port)
    mon.paused.clear()


def main():
    ap = argparse.ArgumentParser(description="NR Intercom: flashen + Konsole")
    ap.add_argument("board", choices=sorted(BOARDS), help="c5 = Bodypack, s3 = Display")
    ap.add_argument("--port", help="COM-Port (sonst Auswahl, der letzte wird gemerkt)")
    ap.add_argument("--monitor", action="store_true", help="nur Konsole, nicht flashen")
    a = ap.parse_args()
    if IS_WIN:
        os.system("")                           # Steuerzeichen (Farben, Zeileneditor) im Fenster aktivieren
    b = BOARDS[a.board]
    print(f"\n  NR Intercom - {b['title']}\n")
    port = choose_port(a.board, a.port)
    if not a.monitor:
        flash(a.board, port)
    mon = Monitor(port)
    mon.t.start()
    try:
        (read_keys_windows if IS_WIN else read_keys_posix)(mon, a.board)
    except KeyboardInterrupt:
        pass
    mon.stop.set()
    mon.close()
    print("\nbeendet.")


if __name__ == "__main__":
    main()
