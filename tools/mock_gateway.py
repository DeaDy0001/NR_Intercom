#!/usr/bin/env python3
"""
NR_Com Device-Gateway - Mock-Server (API v1, siehe docs/device-gateway-api.md)

Zum Testen des Bodypacks, bevor das echte Gateway in NR_Com fertig ist.
Nur Python-Standardbibliothek. Start (Windows):  py tools\\mock_gateway.py
                                     (Linux):    python3 tools/mock_gateway.py

- HTTP auf Port 3002 (/device/v1/hello, login, state, cmd, logout)
- RTP auf Port 40000: Was das Bodypack beim Sprechen schickt, kommt als "Mix" zurueck (Echo),
  sofern das Bodypack in mindestens einem Kanal ist. So hoert man sich selbst mit voller Funk-Latenz.
- Server-Suche: antwortet auf "NRCOM_DISCOVER" an UDP 3010 (Bodypack: Server "automatisch")
- Simulierte Sprecher in den Kanaelen (Aktivitaetsanzeige am Display), abschaltbar mit --no-sim
- Windows-Firewall: Python beim ersten Start fuer "Private Netzwerke" erlauben.
"""
import argparse
import json
import random
import secrets
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

CHANNELS = [
    {"id": 1, "name": "Regie", "color": "#ef4444"},
    {"id": 2, "name": "Ton", "color": "#3b82f6"},
    {"id": 3, "name": "Licht", "color": "#eab308"},
    {"id": 4, "name": "Bühne", "color": "#22c55e"},
    {"id": 5, "name": "Kamera", "color": "#a855f7"},
    {"id": 6, "name": "Video", "color": "#f97316"},
]

lock = threading.Condition()
rev = 1
sessions = {}           # token -> session
talkers = {c["id"]: 0 for c in CHANNELS}
args = None


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def bump():
    global rev
    rev += 1
    lock.notify_all()


def new_session(dev_id, name, ip, rtp_port, ssrc):
    return {
        "deviceId": dev_id, "name": name, "ip": ip, "rtpPort": rtp_port, "ssrc": ssrc,
        "last": time.time(),
        "ch": {c["id"]: {"joined": False, "talk": False, "volume": 100} for c in CHANNELS},
        "rx": 0, "tx": 0,
    }


def state_of(s):
    chans = []
    for c in CHANNELS:
        st = s["ch"][c["id"]]
        chans.append({**c, "joined": st["joined"], "talk": st["talk"], "volume": st["volume"],
                      "talkers": talkers[c["id"]]})
    return {"rev": rev, "user": f'{s["name"]} (Mock)', "channels": chans}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *a):      # leise; wir loggen selbst
        pass

    def send_json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        try:
            return json.loads(self.rfile.read(n) or b"{}")
        except ValueError:
            return {}

    def session(self, token):
        with lock:
            s = sessions.get(token)
            if s:
                s["last"] = time.time()
            return s

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        if u.path == "/device/v1/hello":
            return self.send_json(200, {"api": 1, "name": "NR_Com (Mock)", "project": {"id": 1, "name": "Mock-Projekt"}})
        if u.path == "/device/v1/state":
            token = (q.get("token") or [""])[0]
            since = int((q.get("since") or ["0"])[0] or 0)
            wait = min(int((q.get("wait") or ["0"])[0] or 0), 5000) / 1000.0
            s = self.session(token)
            if not s:
                return self.send_json(401, {"error": "token"})
            with lock:
                end = time.time() + wait
                while rev == since and time.time() < end:
                    lock.wait(end - time.time())
                st = state_of(s)
            return self.send_json(200, st)
        self.send_json(404, {"error": "not-found"})

    def do_POST(self):
        u = urlparse(self.path)
        b = self.body()
        ip = self.client_address[0].replace("::ffff:", "")
        if u.path == "/device/v1/login":
            if str(b.get("pin", "")) != args.pin:
                log(f"Login abgelehnt von {ip}: falsche PIN '{b.get('pin')}'")
                return self.send_json(401, {"error": "pin"})
            token = secrets.token_hex(16)
            with lock:
                for t in [t for t, s in sessions.items() if s["deviceId"] == b.get("deviceId")]:
                    del sessions[t]
                s = new_session(b.get("deviceId", "?"), b.get("deviceName", "Bodypack"), ip,
                                int(b.get("rtpPort", 5004)), int(b.get("ssrc", 0)))
                if args.autojoin:
                    s["ch"][1]["joined"] = True
                sessions[token] = s
                bump()
            log(f"Login: {s['name']} {ip} RTP-Port {s['rtpPort']} SSRC {s['ssrc']:#x}")
            return self.send_json(200, {"token": token, "user": {"id": 7, "name": f"{s['name']} (Mock)"},
                                        "rtp": {"port": args.rtp_port}, "pollMs": 5000})
        s = self.session(b.get("token", ""))
        if not s:
            return self.send_json(401, {"error": "token"})
        if u.path == "/device/v1/logout":
            with lock:
                sessions.pop(b.get("token"), None)
                bump()
            log(f"Logout: {s['name']}")
            return self.send_json(200, {"ok": True})
        if u.path == "/device/v1/cmd":
            cmd, ch, val = b.get("cmd"), b.get("channel"), b.get("value")
            if cmd == "speaking":                       # Sprech-Anzeige, ohne Kanal
                log(f"{s['name']}: spricht {'JA' if val else 'nein'}")
                return self.send_json(200, {"ok": True, "rev": rev})
            if ch not in s["ch"]:
                return self.send_json(404, {"error": "channel"})
            name = next(c["name"] for c in CHANNELS if c["id"] == ch)
            with lock:
                st = s["ch"][ch]
                if cmd == "join":
                    st["joined"], st["talk"] = True, False
                elif cmd == "leave":
                    st["joined"], st["talk"] = False, False
                elif cmd == "talk":
                    if not st["joined"]:
                        return self.send_json(400, {"error": "not-joined"})
                    st["talk"] = bool(val)
                elif cmd == "volume":
                    st["volume"] = max(0, min(100, int(val or 0)))
                else:
                    return self.send_json(400, {"error": "cmd"})
                bump()
                r = rev
            log(f"{s['name']}: {cmd} {name}" + (f" = {val}" if val is not None else ""))
            return self.send_json(200, {"ok": True, "rev": r})
        self.send_json(404, {"error": "not-found"})


def rtp_loop():
    """Echo: Pakete vom Bodypack mit eigener SSRC und PT 100 (wie mediasoup) zuruecksenden."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", args.rtp_port))
    log(f"RTP: UDP-Port {args.rtp_port} (Echo des eigenen Mikrofons, PT 100)")
    last_report = time.time()
    while True:
        data, addr = sock.recvfrom(2048)
        if len(data) < 12:
            continue
        ssrc = struct.unpack("!I", data[8:12])[0]
        with lock:
            s = next((s for s in sessions.values() if s["ssrc"] == ssrc), None)
            if not s:
                continue
            s["rx"] += 1
            joined = any(st["joined"] for st in s["ch"].values())
            dst = (s["ip"], s["rtpPort"])
        if joined:
            out = bytes([data[0], 100]) + data[2:8] + struct.pack("!I", 0xABCDEF01) + data[12:]
            sock.sendto(out, dst)
            s["tx"] += 1
        if time.time() - last_report > 5:
            last_report = time.time()
            log(f"RTP {s['name']}: empfangen {s['rx']}, zurueck {s['tx']}" + ("" if joined else " (in keinem Kanal -> kein Echo)"))


def discovery_loop():
    """Server-Suche: Bodypacks rufen per Broadcast 'NRCOM_DISCOVER 1' an UDP 3010."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("0.0.0.0", args.disc_port))
    except OSError as e:
        log(f"Server-Suche: UDP {args.disc_port} nicht verfuegbar ({e})")
        return
    log(f"Server-Suche: UDP-Port {args.disc_port}")
    reply = json.dumps({"nrcom": 1, "api": 1, "name": "NR_Com (Mock)",
                        "project": {"id": 1, "name": "Mock-Projekt"}, "port": args.port}).encode()
    while True:
        data, addr = sock.recvfrom(512)
        if data.startswith(b"NRCOM_DISCOVER"):
            sock.sendto(reply, addr)
            log(f"Server-Suche von {addr[0]} beantwortet")


def sim_loop():
    """Zufaellige Sprecher in den Kanaelen fuer die Aktivitaetsanzeige."""
    while True:
        time.sleep(random.uniform(1.5, 4))
        c = random.choice(CHANNELS)["id"]
        with lock:
            talkers[c] = 0 if talkers[c] else random.randint(1, 2)
            bump()


def reaper():
    """Sitzungen ohne Lebenszeichen nach 15 s entfernen (wie das echte Gateway)."""
    while True:
        time.sleep(1)
        with lock:
            dead = [t for t, s in sessions.items() if time.time() - s["last"] > 15]
            for t in dead:
                log(f"Zeitueberschreitung: {sessions[t]['name']} getrennt")
                del sessions[t]
            if dead:
                bump()


def local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "?"


def main():
    global args
    p = argparse.ArgumentParser(description="NR_Com Device-Gateway Mock")
    p.add_argument("--port", type=int, default=3002, help="HTTP-Port (Standard 3002)")
    p.add_argument("--rtp-port", type=int, default=40000, help="RTP-Port (Standard 40000)")
    p.add_argument("--disc-port", type=int, default=3010, help="UDP-Port der Server-Suche (Standard 3010)")
    p.add_argument("--pin", default="1234", help="PIN fuer den Login (Standard 1234)")
    p.add_argument("--no-sim", action="store_true", help="keine simulierten Sprecher")
    p.add_argument("--autojoin", action="store_true", help="nach dem Login automatisch in Kanal 'Regie'")
    args = p.parse_args()

    threading.Thread(target=rtp_loop, daemon=True).start()
    threading.Thread(target=reaper, daemon=True).start()
    threading.Thread(target=discovery_loop, daemon=True).start()
    if not args.no_sim:
        threading.Thread(target=sim_loop, daemon=True).start()
    srv = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    srv.daemon_threads = True
    log(f"Mock-Gateway laeuft: http://{local_ip()}:{args.port}/device/v1  (PIN {args.pin})")
    log(f"Am Bodypack: Intercom-Server -> 'Server im Netzwerk suchen' (oder von Hand {local_ip()}, Port {args.port})")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
