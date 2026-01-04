# Trimui Input Daemon

User-space input daemon for the Trimui family (Smart Pro, Smart Pro S, Brick). It reads the device-specific GPIO/serial inputs, publishes a unified uinput gamepad, and now supports rumble on A133-based hardware (Smart Pro / Brick).

## Features
- Auto-detects Smart Pro, Smart Pro S, or Brick at startup.
- Maps physical buttons, triggers, sticks, and hat to a single virtual gamepad.
- Rumble for A133-based units (GPIO 227). Smart Pro S rumble is pluggable but not yet implemented.
- Simple calibration helpers for analog sticks.

## Building

The repo ships with a cross-compilation container. From the project root:

```bash
docker compose up --build
```

If you are using an x86 host, you might need to run this first:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

The resulting binary lives at `build/tsp_inputd/bin/trimui_inputd_smart_pro`.

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
- Rumble: A133 driver uses GPIO 227 active-high. Smart Pro S rumble is reserved for a future driver; the architecture allows another backend to be plugged in without touching device logic.

## Repo layout
- `src/devices/`: Device-specific logic (Smart Pro, Smart Pro S, Brick) and shared rumble core.
- `src/drivers/`: Low-level helpers (serial, GPIO, sunxi GPIO mmap, rumble drivers).
- `src/gamepad/`: uinput wrapper.
- `calibration/`: Stick calibration helpers.
