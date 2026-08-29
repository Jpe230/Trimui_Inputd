# Trimui Input Daemon

User-space input daemon for the Trimui family (Smart Pro, Smart Pro S, Brick). It reads the device-specific GPIO/serial inputs, publishes a unified uinput gamepad, and supports rumble on the supported A133-based hardware.

## Features
- Auto-detects Smart Pro, Smart Pro S, or Brick at startup.
- Maps physical buttons, triggers, sticks, and hat to a single virtual gamepad.
- Rumble for A133-based units through the GPIO 227 and Smart Pro S PWM-vibrator backends.
- Upstream joystick calibration files with per-axis min/max/center values and normalized deadzone support.
- Learned calibration fallback when no upstream calibration file is available.

## Building

The repo ships with a cross-compilation container. From the project root:

```bash
docker compose up --build
```

If you are using an x86 host, you might need to run this first:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

If you prefer a native toolchain, install `gcc`, `make`, and standard headers for your aarch64 rootfs, then run:

```bash
make clean && make
```

The native build outputs to `build/trimui_inputd/bin/trimui_inputd`.

## Running
- Requires access to `/dev/uinput`, serial ports (`/dev/ttyS3/4` or `/dev/ttyAS5/7`), `/dev/mem` (Brick GPIO), and `/sys/class/gpio` (A133 rumble).
- Run the built binary as root (or with the needed capabilities):

```bash
sudo ./build/trimui_inputd/bin/trimui_inputd
```

The daemon auto-detects the connected Trimui variant and starts polling.

## Configuration
- `BRICK_ACTIVE_LOW=0` forces Brick buttons to be treated as active-high (defaults to active-low).
- Joystick calibration lookup checks the first available file in this order: `/etc/trimui/joypad*.config`, `/userdata/system/joypad*.config` (Batocera), then `/mnt/UDISK/joypad*.config` (stock BSP). The concrete filenames are `joypad.config` for the left stick and `joypad_right.config` for the right stick. Override the paths with `TRIMUI_JOYPAD_CONFIG` and `TRIMUI_JOYPAD_RIGHT_CONFIG`. Each file uses `x_min`, `x_max`, `y_min`, `y_max`, `x_zero`, `y_zero`, and a normalized `deadzone` (for example `0.10`). Missing files retain the learned calibration fallback.
- Rumble uses GPIO 227 on the A133 backend and the PWM-vibrator backend on Smart Pro S.

## Repo layout
- `src/devices/`: Device-specific logic (Smart Pro, Smart Pro S, Brick) and shared rumble core.
- `src/drivers/`: Low-level helpers (serial, GPIO, sunxi GPIO mmap, rumble drivers).
- `src/gamepad/`: uinput wrapper.
- `src/calibration/`: Upstream and learned stick calibration helpers.
