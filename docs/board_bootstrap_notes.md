# Board Bootstrap Notes

These notes are based on the Waveshare ESP32-S3-Touch-LCD-4.3B wiki and schematic links provided.

## Important Constraints

- The 4.3-inch RGB display consumes many native GPIOs.
- An I2C IO expander (CH422G) is used by the board for control lines like reset and backlight.
- Multiple onboard peripherals are present (RS485, CAN/TWAI, RTC, TF card).

## Software Strategy

- Keep Phase 1 display-agnostic and focus on robust USB command interface.
- Build functional modules behind CLI commands first.
- Integrate LVGL/HMI only after logger pipeline is stable.

## Immediate Next Commands to Add

- `sd init|status|ls`
- `rtc get|set`
- `sensor start|stop|status`
- `log file start|stop|status`
