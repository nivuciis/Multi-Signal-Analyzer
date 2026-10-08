# Multi-Signal-Analyzer – Firmware

Bare-metal firmware for a low-cost mixed-signal logic analyzer based on the **Raspberry Pi RP2350B**. Signal capture runs on the **PIO** and **DMA** peripherals, so sampling is deterministic and does not depend on the CPU. The device speaks the **sigrok-pico** protocol over USB, so it works directly with **sigrok / PulseView**.

The board design files (schematic, PCB, Gerbers and BOM) are in a separate repository: **[Multi-Signal-Analyzer-Hardware](https://github.com/nivuciis/Multi-Signal-Analyzer-Hardware)**.

---

## Contents

- [Features](#features)
- [How it works](#how-it-works)
- [Channel mapping](#channel-mapping)
- [Repository structure](#repository-structure)
- [Building](#building)
- [Flashing](#flashing)
- [Using with PulseView](#using-with-pulseview)
- [Development tools](#development-tools)
- [Configuration](#configuration)
- [Known limitations](#known-limitations)
- [Citation](#citation)
- [Acknowledgments](#acknowledgments)
- [License](#license)
- [Contributing](#contributing)

---

## Features

- **12 digital channels** sampled in parallel by a PIO state machine
- **3 analog channels** through the RP2350 internal 12-bit ADC
- **RS-485 and RS-232** lines captured as extra digital channels, ready for PulseView's UART decoder
- Sample rate from **5 kHz to 120 MHz**, up to **1,000,000 samples** per capture
- **Hardware triggers** executed inside the PIO: rising edge, falling edge, either edge, high level and low level
- Fixed-length and continuous capture modes
- **Run-length encoding (RLE)** of the sample stream to reduce USB bandwidth
- Bare-metal firmware on the Raspberry Pi Pico C/C++ SDK, **no RTOS**
- Status LED for connection, capture and error states

---

## How it works

### Dual-core architecture

```
Core 0 — USB                              Core 1 — Sigrok processing
───────────────────────────               ─────────────────────────────────
tud_task()  (TinyUSB)                     reads bytes from RX ring
USB CDC  ──►  RX ring  ─────────────────► sigrok command parser
USB CDC  ◄──  TX ring  ◄───────────────── capture control, RLE, responses
detects '+' / '*' and requests abort
```

- **Core 0** only handles USB: it moves bytes between the USB CDC interface and two ring buffers. It also watches for the `+` (stop) and `*` (reset) bytes, so a running capture can be aborted immediately.
- **Core 1** parses sigrok commands, configures and starts the captures, encodes the samples and writes the responses.

### Capture chain

```
GPIO pins ──► PIO state machine ──► RX FIFO ──► DMA ──► SRAM buffer ──► RLE ──► USB CDC ──► PulseView
```

1. **PIO.** A state machine runs `in pins, 12` once per PIO clock, with autopush, so every sample is taken at an exact interval. Its clock divider is set from the sample rate requested by the host.
2. **Triggers.** Five trigger variants wait for the trigger condition using `jmp pin` before entering the sampling loop. Triggering costs no CPU time.
3. **DMA.** A DMA channel paced by the PIO data request moves the samples from the FIFO to a buffer in SRAM.
4. **RLE.** Repeated samples are compressed before transmission. Digital signals are often stable for many samples, which greatly reduces the amount of data sent over USB Full Speed.
5. **USB.** The data is sent using the sigrok-pico protocol and the capture ends with the `$<n>+` marker, where *n* is the number of bytes sent.

### PIO allocation

| PIO block | Function | Source |
|---|---|---|
| `pio0` | 12 digital channels | `pio/capture.pio`, `src/channels.c` |
| `pio1` | RS-485 line | `pio/rs485.pio`, `src/rs485.c` |
| `pio2` | RS-232 lines | `pio/rs232.pio`, `src/rs232.c` |

---

## Channel mapping

The device identifies itself as `SRPICO,A031D15,02`: **3 analog** and **15 digital** channels.

| PulseView channel | Signal | RP2350B GPIO |
|---|---|---|
| D0 – D11 | Digital inputs (20-pin header) | GPIO 8 – 19 |
| D12 | RS-485 (receive) | GPIO 31 |
| D13 – D14 | RS-232 (two receive lines) | GPIO 24 – 25 |
| A0 – A2 | Analog inputs (ADC) | GPIO 45 – 47 |

The pin assignments are defined in `boards/board_def.h`.

---

## Repository structure

```
.
├── boards/        Board definition for the custom hardware (edge_logic_analyzer)
├── includes/      Header files
│   └── handles/   Sigrok protocol handler interfaces
├── pio/           PIO programs (digital capture, RS-485, RS-232)
├── src/           Firmware sources
│   └── handles/   One handler per sigrok command
├── tools/         Python scripts for debugging and testing
├── CMakeLists.txt
└── pico_sdk_import.cmake
```

---

## Building

### Requirements

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) **2.2.0**
- Arm GNU Toolchain (`arm-none-eabi-gcc`), version **14.2** recommended
- CMake **3.13** or newer
- Python 3 (used by the post-build size report)

The board is selected automatically: `CMakeLists.txt` sets `PICO_BOARD=edge_logic_analyzer` and points to the `boards/` folder.

### Option 1: VS Code (recommended)

The project is set up for the official **Raspberry Pi Pico** extension for VS Code, which installs the correct SDK and toolchain versions automatically.

1. Install the *Raspberry Pi Pico* extension in VS Code.
2. Open the repository folder. The extension detects the project.
3. Click **Compile** in the status bar.

### Option 2: Command line

```bash
git clone https://github.com/nivuciis/Multi-Signal-Analyzer.git
cd Multi-Signal-Analyzer

export PICO_SDK_PATH=/path/to/pico-sdk      # Windows (PowerShell): $env:PICO_SDK_PATH="C:\path\to\pico-sdk"

mkdir build
cd build
cmake ..
cmake --build . -j
```

The build produces `build/multi_signal_analyzer.uf2`, ready to flash. It also prints a memory usage report (flash and RAM) and appends it to `build/size_history.csv`.

---

## Flashing

1. Put the RP2350B in **USB bootloader mode (BOOTSEL)**. A drive named `RP2350` appears on the computer.
2. Copy `multi_signal_analyzer.uf2` to that drive.
3. The board reboots automatically and starts the firmware.

Pre-built `.uf2` files are attached to the [Releases](../../releases) page.

### Status LED

| LED state | Meaning |
|---|---|
| Off | Not connected to a host |
| Connected | USB connection with the host is open |
| Capturing | A capture is running |
| Error | Firmware initialization failed |

---

## Using with PulseView

1. Install PulseView from [sigrok.org](https://sigrok.org/wiki/Downloads).
2. Connect the board through USB-C.
3. In PulseView, open **Connect to Device** and choose the driver **RaspberryPI PICO (raspberrypi-pico)**.
4. Select **Serial Port** and the port of the board (`COMx` on Windows, `/dev/ttyACMx` on Linux), then click **Scan for devices**.
5. Choose the channels, sample rate and number of samples, and start the capture.

**Decoding serial buses.** Add PulseView's **UART** decoder on D12 (RS-485) or D13/D14 (RS-232). Use a sample rate of at least **5×** the baud rate, ideally **10×**. Below that, the decoder reports framing errors or garbage bytes.

> **Note:** the USB serial port can be opened by only one program at a time. Close PulseView before running the scripts in `tools/`.

---

## Development tools

The `tools/` folder contains Python scripts that talk to the firmware directly. Most of them need `pyserial`:

```bash
pip install pyserial
```

| Script | Purpose |
|---|---|
| `debug_firmware.py` | Full protocol debugger: identifies the device, configures captures, decodes the RLE stream, shows digital states and analog voltages, and runs self-tests (`--test self`, `--test analog`, `--test trigger`). |
| `probe_device.py` | Sends the same commands as PulseView for a digital capture and dumps the raw stream, to check whether the device is sending valid data. |
| `test_rs485.py` | Continuously sends a string through a USB serial adapter, to generate traffic on the RS-485 bus. |
| `test_rs485_baud.py` | Verifies capture and decoding on the RS-485 channel over a matrix of baud rates and frame formats. |
| `firmware_size_report.py` | Memory usage report, run automatically after each build. |

Examples:

```bash
python3 tools/debug_firmware.py /dev/ttyACM0 --rate 50000 --samples 512
python3 tools/debug_firmware.py --test self
python3 tools/test_rs485.py "hello world" --port /dev/ttyUSB0 --baud 9600
```

---

## Configuration

| Setting | File | Default | Description |
|---|---|---|---|
| `ENABLE_OVERCLOCKING` | `includes/macros.h` | `false` | When enabled, a sample rate request above the maximum raises the core voltage to 1.25 V and the system clock to 250 MHz. This is outside the RP2350 nominal operating conditions; use at your own risk. |
| `SIGROK_SAMPLE_RATE_MIN` / `MAX` | `includes/handles/handles_internal.h` | 5 kHz / 120 MHz | Accepted sample rate range. |
| `SIGROK_SAMPLE_LIMIT_MAX` | `includes/handles/handles_internal.h` | 1,000,000 | Maximum number of samples per capture. |
| Pin assignments | `boards/board_def.h` | — | GPIOs for each channel and interface. |

### Code style

The project uses `clang-format` with the configuration in `.clang-format`. Before committing:

```bash
clang-format -i src/*.c src/handles/*.c includes/*.h includes/handles/*.h
```

---

## Known limitations

- **CAN:** the hardware interface is present on the board, but CAN capture is not yet implemented in the firmware.
- **Analog and digital at the same time:** simultaneous capture is limited at high sample rates because of the RP2350 ADC throughput.
- **Maximum deterministic sample rate:** still under characterization. The goal is a deterministic 50 MHz capture.



---

## Acknowledgments

This project was supported by the Ministry of Science, Technology and Innovation (MCTI), with resources provided under Law No. 8,248/1991, within the scope of the PPI ICT Residency 31, coordinated by Softex.

The host protocol follows the [sigrok-pico](https://github.com/pico-coder/sigrok-pico) project. We thank its authors and the developers and maintainers of sigrok and PulseView.

---

## License

This firmware is released under the [MIT License](LICENSE).

The hardware design files are licensed separately under CERN-OHL-P-2.0, in the [hardware repository](https://github.com/nivuciis/Multi-Signal-Analyzer-Hardware).

---

## Contributing

Feel free to contribute to this project in whatever way works best for you! Every contribution is welcome, for example:

- Reporting bugs or asking questions in the [Issues](../../issues) page
- Suggesting new features, such as support for more protocols
- Submitting pull requests with fixes or improvements
- Testing the firmware with your own setup and sharing your results

If you use this analyzer in a class, lab or project, we would love to hear about it.