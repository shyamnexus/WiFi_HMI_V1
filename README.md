# ESP-IDF Data Logger HMI (Phase 2)

ESP-IDF starter project for **Waveshare ESP32-S3-Touch-LCD-4.3B**.

Current milestone provides USB command-line parser (REPL) and an LVGL HMI table prototype for wireless node telemetry.

## Target Hardware

- Board: Waveshare ESP32-S3-Touch-LCD-4.3B
- MCU: ESP32-S3 (N16R8 variant on product page)
- Primary CLI transport in this phase: USB Serial/JTAG console

## Project Structure

- `main/main.c`: REPL bootstrapping and USB console setup
- `main/cli_parser.c`: command registration and command handlers
- `main/cli_parser.h`: parser interface
- `main/hmi_screen.c`: LVGL table UI with paging controls
- `main/hmi_screen.h`: HMI startup interface
- `main/wireless_data.c`: mock wireless data source feeding the table
- `main/wireless_data.h`: wireless data model API
- `sdkconfig.defaults`: baseline target and console defaults

## Implemented CLI Commands

- `help`: list commands
- `ping`: sanity check, returns `pong`
- `echo <text>`: echoes provided text
- `status`: shows uptime and logger state
- `logger <start|stop|status>`: placeholder logger state control

## LVGL HMI Screen

- Screen title: `Wireless Node Data`
- Table columns: `Node`, `RSSI`, `Battery`, `Samples`, `Seen(s)`
- Supports overflow paging with `Prev` and `Next` buttons
- Footer shows current page indicator (`Page X/Y`)
- Data currently comes from a mock wireless source to validate flow

Replace mock data updates in `wireless_data.c` with real wireless ingestion when the radio transport is integrated.

## Build and Flash

1. Open a terminal in this folder.
2. Set target:
   - `idf.py set-target esp32s3`
3. Build:
   - `idf.py build`
4. Flash and monitor:
   - `idf.py -p <COMx> flash monitor`

If USB monitor does not connect, try entering boot mode using the board BOOT button as documented by Waveshare.

## Notes for Next Phase

- Add SD card storage driver path (TF card)
- Add RTC integration
- Add sensor ingestion tasks
- Add LVGL screen status panel on top of logger backend
- Keep CLI as engineering/service interface
