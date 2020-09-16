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

set(project_dir "${CMAKE_CURRENT_LIST_DIR}/../../")
file(GLOB project_modules ${project_dir}/projects/*)
list(
    APPEND
        CMAKE_MODULE_PATH
        ${project_dir}/kernel
        ${project_dir}/tools/seL4/cmake-tool/helpers/
        ${project_dir}/tools/seL4/elfloader-tool/
        ${project_modules}
)

set(NANOPB_SRC_ROOT_FOLDER "${project_dir}/tools/nanopb" CACHE INTERNAL "")
set(BBL_PATH ${project_dir}/tools/riscv-pk CACHE STRING "BBL Folder location")

set(SEL4_CONFIG_DEFAULT_ADVANCED ON)

include(application_settings)

include(${CMAKE_CURRENT_LIST_DIR}/easy-settings.cmake)

correct_platform_strings()

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()

set(valid_platforms ${KernelPlatform_all_strings} ${correct_platform_strings_platform_aliases})
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
if(NOT "${PLATFORM}" IN_LIST valid_platforms)
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

if(KernelArchARM)
    set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE)
elseif(KernelArchX86)
    set(KernelExportPMCUser ON CACHE BOOL "" FORCE)
endif()

set(KernelDangerousCodeInjection ON CACHE BOOL "" FORCE)

if(KernelPlatformQEMUArmVirt)
    set(SIMULATION ON CACHE BOOL "" FORCE)
endif()

if(SIMULATION)
    ApplyCommonSimulationSettings(${KernelArch})
else()
    if(KernelArchX86)
        set(KernelIOMMU ON CACHE BOOL "" FORCE)
    endif()
endif()

set(KernelIsMCS ON CACHE BOOL "" FORCE)

if(((NOT SIMULATION) OR KernelSel4ArchIA32) AND NOT KernelHardwareDebugAPIUnsupported)
    set(HardwareDebugAPI ON CACHE BOOL "" FORCE)
else()
    set(HardwareDebugAPI OFF CACHE BOOL "" FORCE)
endif()

ApplyCommonReleaseVerificationSettings(${RELEASE} ${VERIFICATION})
