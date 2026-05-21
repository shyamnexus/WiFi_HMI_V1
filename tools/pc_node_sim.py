#!/usr/bin/env python3
import argparse
import random
import socket
import time

FIELD_MAX_LEN = 16


def clip(value: str) -> str:
    return value[:FIELD_MAX_LEN]


def main() -> None:
    parser = argparse.ArgumentParser(description="Send simulated generic node data to HMI over UDP")
    parser.add_argument("--hmi-ip", required=True, help="HMI ESP32 IP address")
    parser.add_argument("--port", type=int, default=7001, help="UDP port used by HMI node ingest")
    parser.add_argument("--node-id", required=True, help="Device name, e.g. NODE-01")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between packets")
    parser.add_argument("--count", type=int, default=0, help="Number of packets to send (0 = run forever)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tick = 0
    sent = 0

    print(f"Sending to {args.hmi_ip}:{args.port} as {args.node_id}")
    while args.count == 0 or sent < args.count:
        tick += 1
        data1 = clip(f"S{(tick % 9) + 1}")
        data2 = clip(f"TEMP{random.randint(20, 60)}")
        data3 = clip(f"LOAD{random.randint(0, 100)}")
        data4 = clip(f"CNT{tick}")

        payload = f"{clip(args.node_id)},{data1},{data2},{data3},{data4}".encode("utf-8")
        sock.sendto(payload, (args.hmi_ip, args.port))
        print(payload.decode("utf-8"))
        sent += 1

        if args.count == 0 or sent < args.count:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
