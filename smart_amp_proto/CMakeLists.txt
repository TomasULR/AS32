cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{ADF_PATH})
    message(FATAL_ERROR "ADF_PATH is not set. Export it to point at your esp-adf checkout, e.g. $env:ADF_PATH = \"$env:USERPROFILE\\esp\\esp-adf\"")
endif()

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(smart_amp_proto C)
