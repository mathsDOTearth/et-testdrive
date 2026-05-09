#include <runtime/IRuntime.h>
#include <runtime/Types.h>
#include <device-layer/IDeviceLayer.h>

#ifdef ET_SYSEMU
#include <sw-sysemu/SysEmuOptions.h>
#endif

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "cannot include the filesystem library"
#endif

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

// Pull in the trace decoder implementation
#define ET_TRACE_DECODER_IMPL
#include <et-trace/decoder.h>
#include <et-trace/layout.h>

#include "Constants.h"

static constexpr uint64_t kShireMask = 0x1; // using one shire
static constexpr size_t kTraceBufferSize = 4096UL * 2048UL; // 8 MB. Way more then enough

static std::vector<std::byte> readFile(const std::string& path) {
    std::ifstream file(path, std::ios_base::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << path << "\n";
        return {};
    }
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> content(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(content.data()), size);
    return content;
}

int main(int argc, char* argv[]) {
    // Determine kernel ELF path
    std::string kernelPath;
    if (argc > 1) {
        kernelPath = argv[1];
    } else {
        kernelPath = fs::path(KERNELS_DIR) / "hello.elf";
    }
    std::cout << "Kernel: " << kernelPath << "\n";

#ifdef ET_SYSEMU
    // Create SysEmu device (not everyone has a physical card)
    emu::SysEmuOptions sysEmuOptions;
    if (fs::exists(BOOTROM_TRAMPOLINE_TO_BL2_ELF)) {
        sysEmuOptions.bootromTrampolineToBL2ElfPath = BOOTROM_TRAMPOLINE_TO_BL2_ELF;
    }
    if (fs::exists(BL2_ELF)) {
        sysEmuOptions.spBL2ElfPath = BL2_ELF;
    }
    if (fs::exists(MACHINE_MINION_ELF)) {
        sysEmuOptions.machineMinionElfPath = MACHINE_MINION_ELF;
    }
    if (fs::exists(MASTER_MINION_ELF)) {
        sysEmuOptions.masterMinionElfPath = MASTER_MINION_ELF;
    }
    if (fs::exists(WORKER_MINION_ELF)) {
        sysEmuOptions.workerMinionElfPath = WORKER_MINION_ELF;
    }
    auto current_path = fs::current_path();
    sysEmuOptions.executablePath = fs::path(SYSEMU_INSTALL_DIR) / "sys_emu";
    sysEmuOptions.runDir = current_path;
    sysEmuOptions.maxCycles = std::numeric_limits<uint64_t>::max();
    sysEmuOptions.minionShiresMask = 0x1FFFFFFFFu;
    sysEmuOptions.puUart0Path = current_path / "pu_uart0_tx.log";
    sysEmuOptions.puUart1Path = current_path / "pu_uart1_tx.log";
    sysEmuOptions.spUart0Path = current_path / "spio_uart0_tx.log";
    sysEmuOptions.spUart1Path = current_path / "spio_uart1_tx.log";
    sysEmuOptions.startGdb = false;

    std::shared_ptr<dev::IDeviceLayer> deviceLayer =
        dev::IDeviceLayer::createSysEmuDeviceLayer(sysEmuOptions, 1);
#else
    // Create PCIe-backed ET-SoC-1 device layer.
    // enableMasterMinion=true opens /dev/et0_ops.
    // enableServiceProcessor=true opens /dev/et0_mgmt.
    std::shared_ptr<dev::IDeviceLayer> deviceLayer =
        dev::IDeviceLayer::createPcieDeviceLayer(true, true);
#endif
    
    // Create runtime, get device, create stream
    auto runtime = rt::IRuntime::create(deviceLayer);
    auto devices = runtime->getDevices();
    if (devices.empty()) {
        std::cerr << "No devices found.\n";
        return 1;
    }
    auto device = devices[0];
    auto stream = runtime->createStream(device);

    // Load kernel binary
    // Unlike OpenCL, ET API does not provide an online compiler. Inestead compiling (either invoke yourself
    // or compile in the build system) prior to use is needed.
    auto elfData = readFile(kernelPath);
    if (elfData.empty()) {
        std::cerr << "Failed to read kernel ELF.\n";
        return 1;
    }
    auto loadResult = runtime->loadCode(stream, elfData.data(), elfData.size());
    runtime->waitForEvent(loadResult.event_);
    auto kernel = loadResult.kernel_;

    // Allocate trace buffer on device.
    // This buffer holds printed data from the device (again, unlike OpenCL, printing does not work by
    // default)
    auto* traceDevBuf = runtime->mallocDevice(device, kTraceBufferSize);

    // Configure kernel launch with tracing (i.e. printing)
    rt::KernelLaunchOptions launchOpts;
    launchOpts.setShireMask(kShireMask);
    launchOpts.setBarrier(true);
    launchOpts.setUserTracing(
        reinterpret_cast<uint64_t>(traceDevBuf),
        static_cast<uint32_t>(kTraceBufferSize),
        0,                              // threshold
        kShireMask,                     // trace shireMask
        0xFFFFFFFFFFFFFFFFULL,          // threadMask — all threads
        0xFFFFFFFFU,                    // eventMask — all events
        0xFFFFFFFFU                     // filterMask — all levels
    );

    // Launch the kernel
    // Unlike OpenCL, again, parameters are a binary blob on ET
    std::vector<std::byte> kernelArgs(64);
    auto launchEvent = runtime->kernelLaunch(
        stream, kernel, kernelArgs.data(), kernelArgs.size(), launchOpts);
    runtime->waitForStream(stream);

    // Copy trace buffer back to host
    std::vector<std::byte> hostTraceBuf(kTraceBufferSize);
    runtime->memcpyDeviceToHost(
        stream, traceDevBuf, hostTraceBuf.data(), kTraceBufferSize);
    runtime->waitForStream(stream);

    // Decode and print kernel prints
    auto* traceHeader = reinterpret_cast<const trace_buffer_std_header_t*>(hostTraceBuf.data());
    const trace_entry_header_t* entry = nullptr;
    int count = 0;

    while ((entry = Trace_Decode(traceHeader, entry))) {
        if (entry->type != TRACE_TYPE_STRING) {
            continue;
        }
        auto* strEntry = reinterpret_cast<const trace_string_t*>(entry);
        std::cout << "[hart " << entry->hart_id << "] " << strEntry->string << "\n";
        ++count;
    }

    std::cout << "Decoded " << count << " trace string entries.\n";

    // Cleanup
    runtime->freeDevice(device, traceDevBuf);
    runtime->unloadCode(kernel);
    runtime->destroyStream(stream);

    return 0;
}
