# et-testdrive: Host program build

find_package(runtime REQUIRED)
find_package(deviceLayer REQUIRED)
find_package(EsperantoBootLoader REQUIRED)
find_package(EsperantoDeviceMinionRuntime REQUIRED)
find_package(esperantoTrace REQUIRED)

if(ET_SYSEMU)
    find_package(sw-sysemu REQUIRED)
endif()


# Resolve firmware ELF paths from imported targets
get_property(BOOTROM_TRAMPOLINE_TO_BL2_ELF TARGET EsperantoBootLoader::BootromTrampolineToBL2.elf PROPERTY LOCATION)
get_property(BL2_ELF TARGET EsperantoBootLoader::ServiceProcessorBL2_fast-boot.elf PROPERTY LOCATION)
get_property(MASTER_MINION_ELF TARGET EsperantoDeviceMinionRuntime::MasterMinion.elf PROPERTY LOCATION)
get_property(MACHINE_MINION_ELF TARGET EsperantoDeviceMinionRuntime::MachineMinion.elf PROPERTY LOCATION)
get_property(WORKER_MINION_ELF TARGET EsperantoDeviceMinionRuntime::WorkerMinion.elf PROPERTY LOCATION)

# Firmware paths
if(NOT SYSEMU_INSTALL_DIR)
    set(SYSEMU_INSTALL_DIR "${CMAKE_PREFIX_PATH}/bin/")
endif()

# Default kernel path points into the build tree (built by ExternalProject)
if(NOT KERNELS_DIR)
    set(KERNELS_DIR "${CMAKE_BINARY_DIR}/kernel")
endif()

# Generate Constants.h with resolved paths
configure_file(Constants.h.in ${CMAKE_CURRENT_BINARY_DIR}/Constants.h)

add_executable(hello_host main.cpp)
target_include_directories(hello_host PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_features(hello_host PRIVATE cxx_std_17)
if(ET_SYSEMU)
    target_compile_definitions(hello_host PRIVATE ET_SYSEMU)
endif()

if(ET_SYSEMU)
    target_link_libraries(hello_host
        PRIVATE
            runtime::etrt
            deviceLayer::deviceLayer
            sw-sysemu::sw-sysemu
            esperantoTrace::et_trace
            $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0.0>>:stdc++fs>
    )
else()
    target_link_libraries(hello_host
        PRIVATE
            runtime::etrt
            deviceLayer::deviceLayer
            esperantoTrace::et_trace
            $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0.0>>:stdc++fs>
    )
endif()

install(TARGETS hello_host
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
