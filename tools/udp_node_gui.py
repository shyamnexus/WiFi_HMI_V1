#!/usr/bin/env python3
import queue
import socket
import threading
import time
import tkinter as tk
from tkinter import messagebox
from tkinter import ttk


DEVICE_COUNT = 32
FIELD_MAX_LEN = 16


class UdpNodeGui(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("ESP HMI UDP Multi-Node Sender")
        self.geometry("1100x760")
        self.minsize(980, 620)

        self.log_queue: queue.Queue[str] = queue.Queue()
        self.stop_event = threading.Event()
        self.sender_thread: threading.Thread | None = None

        self.endpoint_ip = tk.StringVar(value="10.67.97.174")
        self.endpoint_port = tk.StringVar(value="7001")
        self.interval = tk.StringVar(value="1.0")
        self.count = tk.StringVar(value="0")
        self.row_vars: list[dict[str, tk.Variable]] = []

        for index in range(DEVICE_COUNT):
            node_number = index + 1
            node_id = f"NODE-{node_number:02d}"
            data1 = f"D1-{node_number:02d}"
            data2 = f"D2-{node_number:02d}"
            data3 = f"D3-{node_number:02d}"
            data4 = f"D4-{node_number:02d}"
            self.row_vars.append(
                {
                    "enabled": tk.BooleanVar(value=True),
                    "node_id": tk.StringVar(value=node_id),
                    "data1": tk.StringVar(value=data1),
                    "data2": tk.StringVar(value=data2),
                    "data3": tk.StringVar(value=data3),
                    "data4": tk.StringVar(value=data4),
                    "preview": tk.StringVar(),
                }
            )

        self._build_ui()
        self._refresh_previews()
        self.after(100, self._drain_log_queue)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        top = ttk.Frame(self, padding=12)
        top.grid(row=0, column=0, sticky="nsew")
        top.columnconfigure(1, weight=1)
        top.columnconfigure(3, weight=1)

        ttk.Label(top, text="Endpoint IP").grid(row=0, column=0, sticky="w", padx=(0, 8), pady=4)
        ip_entry = ttk.Entry(top, textvariable=self.endpoint_ip)
        ip_entry.grid(row=0, column=1, sticky="ew", pady=4)

        ttk.Label(top, text="Port").grid(row=0, column=2, sticky="w", padx=(16, 8), pady=4)
        port_entry = ttk.Entry(top, textvariable=self.endpoint_port, width=12)
        port_entry.grid(row=0, column=3, sticky="ew", pady=4)

        ttk.Label(top, text="Interval (s)").grid(row=1, column=0, sticky="w", padx=(0, 8), pady=4)
        interval_entry = ttk.Entry(top, textvariable=self.interval)
        interval_entry.grid(row=1, column=1, sticky="ew", pady=4)

        ttk.Label(top, text="Count (0 = forever)").grid(row=1, column=2, sticky="w", padx=(16, 8), pady=4)
        count_entry = ttk.Entry(top, textvariable=self.count)
        count_entry.grid(row=1, column=3, sticky="ew", pady=4)

        ttk.Label(top, text=f"Payload format: device,data1,data2,data3,data4 (max {FIELD_MAX_LEN} chars each)").grid(
            row=2,
            column=0,
            columnspan=4,
            sticky="w",
            pady=(6, 4),
        )

        center = ttk.Frame(self, padding=(12, 0, 12, 12))
        center.grid(row=1, column=0, sticky="nsew")
        center.columnconfigure(0, weight=1)
        center.rowconfigure(1, weight=1)

        button_row = ttk.Frame(center)
        button_row.grid(row=0, column=0, sticky="ew", pady=(0, 10))

        self.send_once_button = ttk.Button(button_row, text=f"Send All {DEVICE_COUNT} Once", command=self.send_once)
        self.send_once_button.grid(row=0, column=0, padx=(0, 8))

        self.start_loop_button = ttk.Button(button_row, text=f"Start All {DEVICE_COUNT} Loop", command=self.start_loop)
        self.start_loop_button.grid(row=0, column=1, padx=(0, 8))

        self.stop_loop_button = ttk.Button(button_row, text="Stop Loop", command=self.stop_loop, state="disabled")
        self.stop_loop_button.grid(row=0, column=2)

        ttk.Label(button_row, text="Tip: use the mouse wheel or scrollbar to navigate all devices.").grid(
            row=0,
            column=3,
            padx=(16, 0),
            sticky="w",
        )

        nodes_frame = ttk.LabelFrame(center, text="Node Packets", padding=10)
        nodes_frame.grid(row=1, column=0, sticky="nsew")
        nodes_frame.columnconfigure(0, weight=1)
        nodes_frame.rowconfigure(0, weight=1)

        canvas = tk.Canvas(nodes_frame, highlightthickness=0)
        canvas.grid(row=0, column=0, sticky="nsew")

        scrollbar = ttk.Scrollbar(nodes_frame, orient="vertical", command=canvas.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        canvas.configure(yscrollcommand=scrollbar.set)

        rows_container = ttk.Frame(canvas)
        self.rows_window = canvas.create_window((0, 0), window=rows_container, anchor="nw")

        rows_container.bind(
            "<Configure>",
            lambda _event: canvas.configure(scrollregion=canvas.bbox("all")),
        )
        canvas.bind(
            "<Configure>",
            lambda event: canvas.itemconfigure(self.rows_window, width=event.width),
        )
        canvas.bind_all("<MouseWheel>", lambda event: canvas.yview_scroll(int(-event.delta / 120), "units"))

        rows_container.columnconfigure(6, weight=1)

        headers = ["Use", "Device", "Data1", "Data2", "Data3", "Data4", "Payload Preview", "Action"]
        for column, text in enumerate(headers):
            ttk.Label(rows_container, text=text).grid(row=0, column=column, sticky="w", padx=4, pady=(0, 6))

        self.row_send_buttons: list[ttk.Button] = []
        for index, row in enumerate(self.row_vars, start=1):
            ttk.Checkbutton(rows_container, variable=row["enabled"]).grid(row=index, column=0, sticky="w", padx=4, pady=4)
            ttk.Entry(rows_container, textvariable=row["node_id"], width=14).grid(row=index, column=1, sticky="ew", padx=4, pady=4)
            ttk.Entry(rows_container, textvariable=row["data1"], width=12).grid(row=index, column=2, sticky="ew", padx=4, pady=4)
            ttk.Entry(rows_container, textvariable=row["data2"], width=12).grid(row=index, column=3, sticky="ew", padx=4, pady=4)
            ttk.Entry(rows_container, textvariable=row["data3"], width=12).grid(row=index, column=4, sticky="ew", padx=4, pady=4)
            ttk.Entry(rows_container, textvariable=row["data4"], width=12).grid(row=index, column=5, sticky="ew", padx=4, pady=4)
            ttk.Label(rows_container, textvariable=row["preview"], font=("Consolas", 10), anchor="w").grid(
                row=index,
                column=6,
                sticky="ew",
                padx=4,
                pady=4,
            )
            send_button = ttk.Button(rows_container, text="Send Row", command=lambda i=index - 1: self.send_row(i))
            send_button.grid(row=index, column=7, sticky="ew", padx=4, pady=4)
            self.row_send_buttons.append(send_button)

        log_frame = ttk.LabelFrame(center, text="Activity Log", padding=8)
        log_frame.grid(row=2, column=0, sticky="nsew", pady=(10, 0))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=12, wrap="word", state="disabled")
        self.log_text.grid(row=0, column=0, sticky="nsew")

        scrollbar = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scrollbar.set)

        for row in self.row_vars:
            for key in ("node_id", "data1", "data2", "data3", "data4"):
                row[key].trace_add("write", self._on_input_changed)

        for variable in (
            self.endpoint_ip,
            self.endpoint_port,
        ):
            variable.trace_add("write", self._on_input_changed)

    def _on_input_changed(self, *_args: object) -> None:
        self._refresh_previews()

    def _refresh_previews(self) -> None:
        for row in self.row_vars:
            row["preview"].set(",".join((
                row["node_id"].get().strip(),
                row["data1"].get().strip(),
                row["data2"].get().strip(),
                row["data3"].get().strip(),
                row["data4"].get().strip(),
            )))

    def _validate_common_inputs(self) -> tuple[str, int, float, int]:
        host = self.endpoint_ip.get().strip()
        if not host:
            raise ValueError("Endpoint IP is required")

        try:
            port = int(self.endpoint_port.get().strip())
        except ValueError as exc:
            raise ValueError("Port must be an integer") from exc

        if port < 1 or port > 65535:
            raise ValueError("Port must be between 1 and 65535")

        try:
            interval = float(self.interval.get().strip())
        except ValueError as exc:
            raise ValueError("Interval must be a number") from exc

        if interval < 0:
            raise ValueError("Interval must be zero or greater")

        try:
            count = int(self.count.get().strip())
        except ValueError as exc:
            raise ValueError("Count must be an integer") from exc

        if count < 0:
            raise ValueError("Count must be zero or greater")

        return host, port, interval, count

    def _validate_row(self, row_index: int) -> tuple[str, str, str, str, str]:
        row = self.row_vars[row_index]

        values = (
            row["node_id"].get().strip(),
            row["data1"].get().strip(),
            row["data2"].get().strip(),
            row["data3"].get().strip(),
            row["data4"].get().strip(),
        )

        for idx, value in enumerate(values, start=1):
            if not value:
                raise ValueError(f"Row {row_index + 1}: Field {idx} is required")
            if len(value) > FIELD_MAX_LEN:
                raise ValueError(f"Row {row_index + 1}: Field {idx} exceeds {FIELD_MAX_LEN} chars")

        return values

    def _active_row_indexes(self) -> list[int]:
        indexes = [index for index, row in enumerate(self.row_vars) if row["enabled"].get()]
        if not indexes:
            raise ValueError("Enable at least one row to send")
        return indexes

    def _build_payload(self, device: str, data1: str, data2: str, data3: str, data4: str) -> str:
        return f"{device},{data1},{data2},{data3},{data4}"

    def _send_packet(self, host: str, port: int, payload: str) -> None:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(payload.encode("utf-8"), (host, port))

    def send_row(self, row_index: int) -> None:
        try:
            host, port, _interval, _count = self._validate_common_inputs()
            device, data1, data2, data3, data4 = self._validate_row(row_index)
            payload = self._build_payload(device, data1, data2, data3, data4)
            self._send_packet(host, port, payload)
        except OSError as exc:
            messagebox.showerror("Send failed", str(exc))
            return
        except ValueError as exc:
            messagebox.showerror("Invalid input", str(exc))
            return

        self._log(f"Sent row {row_index + 1} -> {host}:{port} | {payload}")

    def send_once(self) -> None:
        try:
            host, port, _interval, _count = self._validate_common_inputs()
            row_indexes = self._active_row_indexes()
            payloads: list[tuple[int, str]] = []
            for row_index in row_indexes:
                device, data1, data2, data3, data4 = self._validate_row(row_index)
                payload = self._build_payload(device, data1, data2, data3, data4)
                self._send_packet(host, port, payload)
                payloads.append((row_index, payload))
        except OSError as exc:
            messagebox.showerror("Send failed", str(exc))
            return
        except ValueError as exc:
            messagebox.showerror("Invalid input", str(exc))
            return

        for row_index, payload in payloads:
            self._log(f"Sent row {row_index + 1} -> {host}:{port} | {payload}")

    def start_loop(self) -> None:
        if self.sender_thread and self.sender_thread.is_alive():
            return

        try:
            host, port, interval, count = self._validate_common_inputs()
            row_indexes = self._active_row_indexes()
            row_data = []
            for row_index in row_indexes:
                device, data1, data2, data3, data4 = self._validate_row(row_index)
                row_data.append((row_index, device, data1, data2, data3, data4))
        except ValueError as exc:
            messagebox.showerror("Invalid input", str(exc))
            return

        self.stop_event.clear()
        self._set_loop_buttons(running=True)
        self.sender_thread = threading.Thread(
            target=self._run_loop,
            args=(host, port, row_data, interval, count),
            daemon=True,
        )
        self.sender_thread.start()
        self._log(
            f"Loop started -> {host}:{port} rows={','.join(str(row_index + 1) for row_index, *_ in row_data)} "
            f"count={count} interval={interval:.3f}s"
        )

    def stop_loop(self) -> None:
        self.stop_event.set()
        self._set_loop_buttons(running=False)
        self._log("Loop stop requested")

    def _run_loop(
        self,
        host: str,
        port: int,
        row_data: list[tuple[int, str, str, str, str, str]],
        interval: float,
        count: int,
    ) -> None:
        sent = 0
        row_states = [list(item) for item in row_data]

        try:
            while not self.stop_event.is_set() and (count == 0 or sent < count):
                for row_state in row_states:
                    if self.stop_event.is_set() or (count != 0 and sent >= count):
                        break

                    row_index, device, data1, data2, data3, data4 = row_state
                    payload = self._build_payload(device, data1, data2, data3, data4)
                    self._send_packet(host, port, payload)
                    sent += 1
                    self.log_queue.put(f"Sent row {row_index + 1} -> {host}:{port} | {payload}")

                if interval > 0:
                    if self.stop_event.wait(interval):
                        break
        except OSError as exc:
            self.log_queue.put(f"Error -> {exc}")
        finally:
            self.after(0, lambda: self._set_loop_buttons(running=False))
            self.log_queue.put(f"Loop finished after {sent} packet(s)")

    def _set_loop_buttons(self, running: bool) -> None:
        self.send_once_button.configure(state="disabled" if running else "normal")
        self.start_loop_button.configure(state="disabled" if running else "normal")
        self.stop_loop_button.configure(state="normal" if running else "disabled")
        for button in self.row_send_buttons:
            button.configure(state="disabled" if running else "normal")

    def _log(self, message: str) -> None:
        self.log_text.configure(state="normal")
        self.log_text.insert("end", f"{time.strftime('%H:%M:%S')}  {message}\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def _drain_log_queue(self) -> None:
        while True:
            try:
                message = self.log_queue.get_nowait()
            except queue.Empty:
                break
            self._log(message)
        self.after(100, self._drain_log_queue)

    def _on_close(self) -> None:
        self.stop_event.set()
        self.destroy()


def main() -> None:
    app = UdpNodeGui()
    app.mainloop()


if __name__ == "__main__":
    main()