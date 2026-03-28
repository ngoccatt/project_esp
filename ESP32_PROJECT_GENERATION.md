# Generating a TFLM C++ Library Tree for ESP32

The TensorFlow Lite Micro (TFLM) repository does not ship pre-built sources. Instead, a Python script assembles the correct sources and headers for your target. This document explains exactly how to do that.

---

## Prerequisites

- Python 3
- `make` (≥ 3.82)
- `unzip`, `curl`, `openssl` (used by the download scripts)
- This must be run on a Linux environment (using WSL is okey)

Clone the tflite-micro first
```bash
git clone https://github.com/tensorflow/tflite-micro.git
```

Install missing tools on Ubuntu/Debian:
```bash
apt-get install -y make unzip curl openssl
```

---

## Step 1 — Download Third-Party Dependencies

The build system needs flatbuffers, kissfft, ruy, and gemmlowp before it can list source files.

```bash
cd /path/to/tflite-micro
make -f tensorflow/lite/micro/tools/make/Makefile third_party_downloads
```

Downloads are placed in `tensorflow/lite/micro/tools/make/downloads/`.

> **Note:** If you see `unzip: command not found`, install it and re-run. The download script will silently create an empty directory on failure, so verify each subdirectory inside `downloads/` is non-empty before proceeding.

---

## Step 2 — Generate the Source Tree

Run the project generation script. The `--rename_cc_to_cpp` flag is required for ESP32/Arduino toolchains which do not recognise `.cc` files.

```bash
python3 tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py \
  --rename_cc_to_cpp \
  --examples hello_world \
  -e person_detection \
  /path/to/output/tflm_esp32
```

### Available options

| Option | Description |
|---|---|
| `--rename_cc_to_cpp` | Rename all `.cc` files to `.cpp` (required for ESP32) |
| `--examples hello_world` | Include the hello_world example. Can be repeated: `-e hello_world -e micro_speech` |
| `--makefile_options="..."` | Pass extra Makefile variables, e.g. `TARGET_ARCH=xtensa` |
| `--print_src_files` | Dry-run: print source files without copying |
| `--no_copy` | Dry-run: resolve files but do not copy |

### Output layout

```
tflm_esp32/
  tensorflow/         ← TFLM core library sources and headers
  signal/             ← signal processing kernels
  third_party/
    flatbuffers/
    gemmlowp/
    kissfft/
    ruy/
  examples/
    hello_world/
    person_detection/
  LICENSE
```

---

## Step 3 — Integrate into Your ESP32 Project (ESP-IDF)

Copy or symlink the generated tree into your project's `components/` directory, then add a `CMakeLists.txt`:

### Directory structure

```
my_esp32_project/
  components/
    tflm/             ← the generated tflm_esp32 tree
      CMakeLists.txt  ← create this file (see below)
      tensorflow/
      signal/
      third_party/
      ...
  main/
    CMakeLists.txt
    main.cpp
  CMakeLists.txt
```

### `components/tflm/CMakeLists.txt`

```cmake
file(GLOB_RECURSE TFLM_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/tensorflow/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tensorflow/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/signal/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/signal/*.c"
)

idf_component_register(
  SRCS ${TFLM_SRCS}
  INCLUDE_DIRS
    "."
    "third_party/flatbuffers/include"
    "third_party/gemmlowp"
    "third_party/kissfft"
    "third_party/ruy"
  COMPILE_OPTIONS
    "-DTF_LITE_STATIC_MEMORY"
    "-DTF_LITE_DISABLE_X86_NEON"
    "-fno-exceptions"
    "-fno-rtti"
)
```

### `main/CMakeLists.txt`

```cmake
idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS "."
  REQUIRES tflm
)
```

### Build

```bash
cd my_esp32_project
idf.py set-target esp32
idf.py build
```

---

## Alternative: PlatformIO / Arduino IDE

Place the generated tree under `lib/tflm/` in your PlatformIO project and add to `platformio.ini`:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
  -DTF_LITE_STATIC_MEMORY
  -DTF_LITE_DISABLE_X86_NEON
  -fno-exceptions
  -fno-rtti
lib_extra_dirs = lib/tflm
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `no rule to make target 'generate_projects'` | That target does not exist in this Makefile | Use the Python script in Step 2 instead |
| `unzip: command not found` | `unzip` not installed | `apt-get install -y unzip` |
| `FileNotFoundError: .../gemmlowp/fixedpoint/fixedpoint.h` | download failed silently, leaving empty dir | `rmdir downloads/gemmlowp && make ... third_party_downloads` |
| `.cc` files not compiled by ESP-IDF | ESP toolchain ignores `.cc` by default | Always use `--rename_cc_to_cpp` |
