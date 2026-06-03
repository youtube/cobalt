# Copyright 2020 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.3)

project(Cobalt)

find_package(WPEFramework)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(PLUGIN_NAME Cobalt)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_COBALT_IMPLEMENTATION "${MODULE_NAME}Impl" CACHE STRING "Specify a library with a Cobalt implementation." )
set(PLUGIN_COBALT_AUTOSTART false CACHE STRING "Automatically start Cobalt plugin")
set(PLUGIN_COBALT_CLOSUREPOLICY "quit" CACHE STRING "Configures how to handle window close request")
set(PLUGIN_COBALT_AUTOSTART "false" CACHE STRING "Automatically start Cobalt plugin")
set(PLUGIN_COBALT_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_COBALT_LIMIT_AD_TRACKING false CACHE STRING "Disable Ad Tracking and measurement.")
set(PLUGIN_COBALT_EXTERNAL_COBALT_LIBRARY "" CACHE STRING "Path of libloader_app.so for external build" )

option(PLUGIN_COBALT_ENABLE_FOCUS_IFACE "Enable support for focus change." OFF)
option(PLUGIN_COBALT_EVEGREEN_LITE "Enable Evergreen Lite build configuration." OFF)
option(PLUGIN_COBALT_EVEGREEN_COMPATIBLE "Enable Evergreen compatible configuration." OFF)

if(PLUGIN_COBALT_ENABLE_FOCUS_IFACE)
    add_definitions(-DPLUGIN_COBALT_ENABLE_FOCUS_IFACE=1)
endif()

if(NOT PLUGIN_COBALT_CLOSUREPOLICY)
    add_definitions(-DPLUGIN_COBALT_ENABLE_CLOSUREPOLICY=0)
else()
    add_definitions(-DPLUGIN_COBALT_ENABLE_CLOSUREPOLICY=1)
endif(NOT PLUGIN_COBALT_CLOSUREPOLICY)

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)

# find_package(JsonGenerator REQUIRED)
# JsonGenerator(DOCS
#   INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/CobaltPlugin.json
#   IFDIR  ${CMAKE_CURRENT_SOURCE_DIR}/interfaces/json/
#   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/doc/
# )
# JsonGenerator(CODE STUBS
#   INPUT  ${CMAKE_CURRENT_SOURCE_DIR}/CobaltPlugin.json
#   IFDIR  ${CMAKE_CURRENT_SOURCE_DIR}/interfaces/json/
#   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/interfaces/json/
# )

add_library(${MODULE_NAME} SHARED
    Cobalt.cpp
    CobaltJsonRpc.cpp
    Module.cpp
    )

if (PLUGIN_COBALT_EVEGREEN_LITE)
    add_definitions(-DENABLE_EVERGREEN_LITE=1)
    set(PLUGIN_COBALT_EVEGREEN_COMPATIBLE ON)
endif()

if (PLUGIN_COBALT_EVEGREEN_COMPATIBLE)
  if (NOT "${PLUGIN_COBALT_EXTERNAL_COBALT_LIBRARY}" STREQUAL "")
    set(COBALT_LIBRARY ${PLUGIN_COBALT_EXTERNAL_COBALT_LIBRARY})
  else()
    find_library(COBALT_LIBRARY loader_app REQUIRED)
  endif()
else()
    find_library(COBALT_LIBRARY cobalt REQUIRED)
endif()

message(STATUS "Cobalt library path: ${COBALT_LIBRARY}")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    )

add_library(${PLUGIN_COBALT_IMPLEMENTATION} SHARED
    Module.cpp
    CobaltImplementation.cpp
    )

target_link_libraries(${PLUGIN_COBALT_IMPLEMENTATION}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${COBALT_LIBRARY}
    )

# Library installation section
string(TOLOWER ${NAMESPACE} STORAGENAME)
install(
    TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGENAME}/plugins
    COMPONENT ${PROJECT_NAME})
install(
    TARGETS ${PLUGIN_COBALT_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGENAME}/plugins
    COMPONENT ${PROJECT_NAME})

write_config()
