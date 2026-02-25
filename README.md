# et-testdrive: Minimal-ish Hello World for ET-SoC-1

A minimal project that demonstrates launching a device kernel on a single shire and reading back trace (kenel prints) output using ET platform APIs.

## What it does

- **Device kernel** (`kernel/hello.c`): Every hart calls `et_printf("Hello World from hart %d\n", get_hart_id())` and returns 0.
- **Host program** (`host/main.cpp`): Creates a SysEmu device, loads the kernel, launches it on shire 0x1 with full user tracing, copies the trace buffer back, decodes it, and prints each string entry.

## Building

Requires a completed et-platform build with packages installed to `/opt/et`. See [et-platform](https://github.com/aifoundry-org/et-platform) for setup instructions

```bash
cd ~/Documents/et-testdrive
cmake -B build -DCMAKE_PREFIX_PATH=/opt/et -Wno-dev
cmake --build build
```

This builds both the RISC-V device kernel (cross-compiled via ExternalProject) and the x86 host program in a single command.

## Running

```bash
./build/host/hello_host ./build/kernel/hello.elf
```

## Project structure

```
et-testdrive/
├── CMakeLists.txt          # Top-level: ExternalProject for kernel + host subdirectory
├── README.md
├── kernel/
│   ├── CMakeLists.txt      # RISC-V cross-compiled device kernel
│   ├── crt.S               # Startup code (_start -> entry_point -> ecall)
│   ├── hello.c             # Device kernel source
│   └── sections.ld         # Linker script (places kernel at KERNEL_UMODE_ENTRY)
└── host/
    ├── CMakeLists.txt      # Host program build
    ├── Constants.h.in      # Template for firmware ELF paths
    └── main.cpp            # Host program source
```
