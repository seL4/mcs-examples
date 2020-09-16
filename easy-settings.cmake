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

set(SIMULATION ON CACHE BOOL "Include only simulation compatible tests")
set(RELEASE OFF CACHE BOOL "Performance optimized build")
set(VERIFICATION OFF CACHE BOOL "Only verification friendly kernel features")
set(BAMBOO OFF CACHE BOOL "Enable machine parseable output")
set(DOMAINS OFF CACHE BOOL "Test multiple domains")
set(SMP OFF CACHE BOOL "(if supported) Test SMP kernel")
set(NUM_NODES "" CACHE STRING "(if SMP) the number of nodes (default 4)")
set(PLATFORM "x86_64" CACHE STRING "Platform to test")
set(ARM_HYP OFF CACHE BOOL "Hyp mode for ARM platforms")
set(MCS ON CACHE BOOL "MCS kernel")
set(KernelSel4Arch "" CACHE STRING "aarch32, aarch64, arm_hyp, ia32, x86_64, riscv32, riscv64")
