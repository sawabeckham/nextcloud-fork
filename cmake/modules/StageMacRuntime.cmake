# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.16)
foreach(required INPUT OUTPUT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing runtime staging argument: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "Runtime library does not exist: ${INPUT}")
endif()
get_filename_component(destination "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${destination}")
configure_file("${INPUT}" "${OUTPUT}" COPYONLY)
