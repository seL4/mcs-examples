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

include(settings.cmake)

project(mcs-examples C CXX ASM)

if(CAMKES_APP)
    add_subdirectory(apps/mcs-camkes)
else()
    add_subdirectory(apps/mcs-native)
endif()

include(simulation)
if(KernelSel4ArchX86_64)
    SetSimulationScriptProperty(MEM_SIZE "3G")
endif()
if(KernelPlatformQEMUArmVirt)
    SetSimulationScriptProperty(MEM_SIZE "2G")
endif()

if(SIMULATION)
    GenerateSimulateScript()
endif()
