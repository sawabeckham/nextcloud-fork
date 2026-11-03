find_package(Qt6 ${REQUIRED_QT_VERSION} COMPONENTS REQUIRED Core Test Xml Network Qml Quick QuickTest)

# SPDX-FileCopyrightText: 2028 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2012 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

macro(nextcloud_build_test test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt::Test
      Qt::Quick
      Qt::Core5Compat
    )

    target_compile_definitions(${OWNCLOUD_TEST_CLASS}Test PRIVATE QT_NO_KEYWORDS)

    if (WIN32)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE nextcloudsync_vfs_cfapi)
    elseif (LINUX)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE nextcloudsync_vfs_xattr)
    endif()

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE updater)
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")

    target_include_directories(${OWNCLOUD_TEST_CLASS}Test PRIVATE
        "${CMAKE_SOURCE_DIR}/test/"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer
    )
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES FOLDER Tests)
endmacro()

macro(nextcloud_add_test test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Test test${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt::Test
      Qt::Quick
      Qt::Core5Compat
    )

    target_compile_definitions(${OWNCLOUD_TEST_CLASS}Test PRIVATE QT_NO_KEYWORDS)

    if (WIN32)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            nextcloudsync_vfs_cfapi
        )
    endif()

    if (LINUX)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            nextcloudsync_vfs_xattr
        )
    endif()

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Test PRIVATE
            updater
        )
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
    add_test(NAME ${OWNCLOUD_TEST_CLASS}Test
        COMMAND ${OWNCLOUD_TEST_CLASS}Test
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

    target_include_directories(${OWNCLOUD_TEST_CLASS}Test
        PRIVATE
        "${CMAKE_SOURCE_DIR}/test/"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer
        )
    set_target_properties(${OWNCLOUD_TEST_CLASS}Test PROPERTIES FOLDER Tests)
endmacro()

macro(nextcloud_add_benchmark test_class)
    set(CMAKE_AUTOMOC TRUE)
    set(OWNCLOUD_TEST_CLASS ${test_class})
    string(TOLOWER "${OWNCLOUD_TEST_CLASS}" OWNCLOUD_TEST_CLASS_LOWERCASE)

    add_executable(${OWNCLOUD_TEST_CLASS}Bench benchmarks/bench${OWNCLOUD_TEST_CLASS_LOWERCASE}.cpp)
    set_target_properties(${OWNCLOUD_TEST_CLASS}Bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
      Nextcloud::sync
      testutils
      nextcloudCore
      cmdCore
      Qt::Core
      Qt::Test
      Qt::Xml
      Qt::Network
      Qt::Core5Compat
    )

    target_compile_definitions(${OWNCLOUD_TEST_CLASS}Bench PRIVATE QT_NO_KEYWORDS)

    IF(BUILD_UPDATER)
        target_link_libraries(${OWNCLOUD_TEST_CLASS}Bench
            updater
        )
    endif()

    add_definitions(-DOWNCLOUD_TEST)
    add_definitions(-DOWNCLOUD_BIN_PATH="${CMAKE_BINARY_DIR}/bin")
    set_target_properties(${OWNCLOUD_TEST_CLASS}Bench PROPERTIES FOLDER Tests/Benchmarks)
endmacro()

# The application plist template identifies every bundle as the real client.
# Native services must see each test executable as a separate application.
function(nextcloud_isolate_macos_test_bundles directory)
    get_property(test_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(test_target IN LISTS test_targets)
        get_target_property(test_target_type "${test_target}" TYPE)
        if(test_target_type STREQUAL "EXECUTABLE")
            set_target_properties("${test_target}" PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_ROOT}/Modules/MacOSXBundleInfo.plist.in"
                MACOSX_BUNDLE_GUI_IDENTIFIER "${APPLICATION_REV_DOMAIN}.tests.${test_target}"
                MACOSX_BUNDLE_BUNDLE_NAME "${test_target}")
        endif()
    endforeach()
    get_property(test_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(test_subdirectory IN LISTS test_subdirectories)
        nextcloud_isolate_macos_test_bundles("${test_subdirectory}")
    endforeach()
endfunction()
