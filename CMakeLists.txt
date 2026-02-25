# et-testdrive: Minimal Hello World — Device Kernel + Host Program
#
# Single-command build:
#
#   cmake -B build -DCMAKE_PREFIX_PATH=/opt/et
#   cmake --build build
#
# Then run:
#
#   ./build/host/hello_host ./build/kernel/hello.elf

cmake_minimum_required(VERSION 3.17)
project(et-testdrive VERSION 0.1.0)

include(ExternalProject)

# Pick up Find*.cmake modules (Findlibcap, Findgflags, etc.)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_PREFIX_PATH}/lib/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_PREFIX_PATH}/lib/cmake/cmake-modules")

# --- Device kernel (cross-compiled via ExternalProject) ---
set(KERNEL_BINARY_DIR ${CMAKE_BINARY_DIR}/kernel)
set(HELLO_ELF ${KERNEL_BINARY_DIR}/hello.elf)

ExternalProject_Add(hello_kernel
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel
    BINARY_DIR ${KERNEL_BINARY_DIR}
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_PREFIX_PATH}/lib/cmake/riscv64-ec-toolchain.cmake
        -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        -DCMAKE_MODULE_PATH=${CMAKE_PREFIX_PATH}/lib/cmake/cmake-modules
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_LIBDIR=lib
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${HELLO_ELF}
)

# --- Host program ---
add_subdirectory(host)
add_dependencies(hello_host hello_kernel)
