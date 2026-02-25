# et-testdrive: Device kernel build
#
# Cross-compile with:
#   cd ~/Documents/et-testdrive
#   cmake -B build-kernel \
#     -DCMAKE_TOOLCHAIN_FILE=/opt/et/lib/cmake/riscv64-ec-toolchain.cmake \
#     -DCMAKE_PREFIX_PATH=/opt/et \
#     -DCMAKE_MODULE_PATH=/opt/et/lib/cmake/cmake-modules \
#     -DCMAKE_INSTALL_PREFIX=/opt/et \
#     -DCMAKE_INSTALL_LIBDIR=lib \
#     kernel/
#   cmake --build build-kernel

cmake_minimum_required(VERSION 3.17)
project(et-testdrive-kernel LANGUAGES C CXX ASM)

include(GNUInstallDirs)
include(DeviceUtils)

find_package(et-common-libs REQUIRED)
find_package(esperantoTrace REQUIRED)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/sections.ld)

add_riscv_executable(hello)
target_sources(hello.elf PRIVATE hello.c crt.S)
target_link_libraries(hello.elf
    PRIVATE
        et-common-libs::cm-umode
        esperantoTrace::et_trace
)
set_target_properties(hello.elf
    PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
)

install(TARGETS hello.elf
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/esperanto-fw/kernels
)
