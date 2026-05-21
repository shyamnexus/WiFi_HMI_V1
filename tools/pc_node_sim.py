#!/usr/bin/env python3
import argparse
import random
import socket
import time


def main() -> None:
    parser = argparse.ArgumentParser(description="Send simulated node data to HMI over UDP")
    parser.add_argument("--hmi-ip", required=True, help="HMI ESP32 IP address")
    parser.add_argument("--port", type=int, default=7001, help="UDP port used by HMI node ingest")
    parser.add_argument("--node-id", required=True, help="Node id, e.g. NODE-01")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between packets")
    parser.add_argument("--count", type=int, default=0, help="Number of packets to send (0 = run forever)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    samples = 100
    sent = 0

    print(f"Sending to {args.hmi_ip}:{args.port} as {args.node_id}")
    while args.count == 0 or sent < args.count:
        rssi = random.randint(-85, -45)
        battery = round(random.uniform(3.55, 4.20), 2)
        samples += random.randint(1, 4)

        payload = f"{args.node_id},{rssi},{battery:.2f},{samples}".encode("utf-8")
        sock.sendto(payload, (args.hmi_ip, args.port))
        print(payload.decode("utf-8"))
        sent += 1

        if args.count == 0 or sent < args.count:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
