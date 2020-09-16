#
# Copyright 2020, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

project(mcs-native C ASM)

find_package(elfloader-tool REQUIRED)
find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)

sel4_import_kernel()

set(KernelRootCNodeSizeBits 13 CACHE INTERNAL "")

if((KernelArchARM OR KernelArchRiscV))
    # Elfloader settings that correspond to how Data61 sets its boards up.
    ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
endif()
elfloader_import_project()

# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()

add_executable(mcs-native main.c)

target_link_libraries(
    mcs-native
    sel4
    muslc
    utils
    sel4muslcsys
    sel4platsupport
    sel4utils
    sel4debug
    sel4allocman
    sel4bench
    sel4runtime
    sel4debug
)

include(rootserver)
DeclareRootserver(mcs-native)
