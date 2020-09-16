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

find_package(camkes-tool REQUIRED)
find_package(global-components REQUIRED)
find_package(sel4_projects_libs REQUIRED)

set(KernelRootCNodeSizeBits 17 CACHE STRING "")

camkes_tool_setup_camkes_build_environment()

sel4_projects_libs_import_libraries()

function(includeGlobalComponents)
    global_components_import_project()
endfunction()

project(mcs-scheduling C)

DeclareCAmkESComponent(Task SOURCES components/Task/src/task.c LIBS sel4bench)
DeclareCAmkESComponent(Init SOURCES components/Init/src/init.c LIBS sel4bench)
DeclareCAmkESRootserver(mcs-scheduling.camkes)

GenerateCAmkESRootserver()
