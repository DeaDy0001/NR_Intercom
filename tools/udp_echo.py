#!/usr/bin/env python3
"""UDP-Echo-Server fuer Bodypack-Tests (laeuft auf dem PC, Windows/Linux/macOS).

Schickt jedes empfangene UDP-Paket sofort an den Absender zurueck.

  Port 5007: Funk-Messung      (Bodypack-Befehl: nettest <pc-ip>)
  Port 5006: RTP-Echo-Test     (Bodypack: mode rtp, rtp <pc-ip> 5006, Taste 1 halten
                                -> man hoert die eigene Stimme ueber das WLAN zurueck)

Aufruf:  python udp_echo.py            (beide Ports)
         python udp_echo.py --ports 5007
Es werden nur Python-Standardmodule benutzt.
"""
import argparse
import selectors
import socket
import time


def main() -> None:
    ap = argparse.ArgumentParser(description="UDP-Echo fuer NR Bodypack")
    ap.add_argument("--ports", type=int, nargs="+", default=[5006, 5007])
    ap.add_argument("--bind", default="0.0.0.0")
    args = ap.parse_args()

    sel = selectors.DefaultSelector()
    stats = {}
    for port in args.ports:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.setsockopt(socket.IPPROTO_IP, socket.IP_TOS, 0xB8)  # DSCP EF
        except OSError:
            pass
        s.bind((args.bind, port))
        s.setblocking(False)
        sel.register(s, selectors.EVENT_READ, port)
        stats[port] = 0
        print(f"Echo aktiv auf UDP {args.bind}:{port}")

    print("Beenden mit Strg+C\n")
    last = time.monotonic()
    peers = set()
    try:
        while True:
            for key, _ in sel.select(timeout=1.0):
                sock = key.fileobj
                while True:
                    try:
                        data, addr = sock.recvfrom(2048)
                    except BlockingIOError:
                        break
                    sock.sendto(data, addr)
                    stats[key.data] += 1
                    peers.add(addr[0])
            now = time.monotonic()
            if now - last >= 5.0:
                line = ", ".join(f"Port {p}: {n / (now - last):.0f} Pakete/s" for p, n in stats.items())
                print(f"{time.strftime('%H:%M:%S')}  {line}  Geraete: {', '.join(sorted(peers)) or '-'}")
                for p in stats:
                    stats[p] = 0
                last = now
    except KeyboardInterrupt:
        print("\nbeendet")


if __name__ == "__main__":
    main()
