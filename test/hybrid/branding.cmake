# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.16)
set(CMAKE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(NEXTCLOUD_FORK ON)
set(APPLE FALSE)
include("${CMAKE_SOURCE_DIR}/NEXTCLOUD.cmake")
if(NOT APPLICATION_NAME STREQUAL "Nextcloud Fork"
   OR NOT APPLICATION_EXECUTABLE STREQUAL "nextcloudfork"
   OR NOT APPLICATION_CONFIG_NAME STREQUAL "nextcloudfork"
   OR NOT APPLICATION_REV_DOMAIN STREQUAL "site.manro.nextcloudfork"
   OR BUILD_UPDATER
   OR NOT APPLICATION_UPDATE_URL STREQUAL "about:blank"
   OR DEVELOPMENT_TEAM STREQUAL "NKUJUXUJ3B")
    message(FATAL_ERROR "Fork branding is not isolated")
endif()
set(NEXTCLOUD_FORK OFF)
include("${CMAKE_SOURCE_DIR}/NEXTCLOUD.cmake")
if(NOT APPLICATION_NAME STREQUAL "Nextcloud")
    message(FATAL_ERROR "Default branding changed")
endif()

# An unsigned fork still needs a valid C++ string for its absent team identifier.
set(DEVELOPMENT_TEAM "")
configure_file("${CMAKE_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/fork-config-test.h" @ONLY)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/fork-config-test.h" _header)
string(FIND "${_header}" "#define DEVELOPMENT_TEAM \"\"" _team_offset)
if(_team_offset EQUAL -1)
    message(FATAL_ERROR "Empty development team must remain a C++ string")
endif()
