cmake_minimum_required(VERSION 4.0)

project(
  PlacementDB
  VERSION 0.1.0
  DESCRIPTION "Community placement question bank for SRM students"
  LANGUAGES CXX
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "PlacementDB currently supports Linux only")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)
include(PlacementDBSanitizers)
include(PlacementDBWarnings)

option(PLACEDB_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(PLACEDB_ENABLE_ASAN "Enable AddressSanitizer for project targets" OFF)
option(PLACEDB_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for project targets" OFF)
option(PLACEDB_BUILD_DEMO_SERVER "Build the database-free Drogon demo server" ON)
option(PLACEDB_BUILD_ISOLATED_TESTS "Build isolated project test targets" ON)

if(PLACEDB_ENABLE_ASAN AND PLACEDB_ENABLE_UBSAN)
  message(FATAL_ERROR "ASan and UBSan must use separate presets")
endif()

add_library(placedb_project_options INTERFACE)
add_library(PlacementDB::ProjectOptions ALIAS placedb_project_options)

target_compile_features(placedb_project_options INTERFACE cxx_std_23)
placedb_enable_warnings(placedb_project_options)
placedb_enable_sanitizers(placedb_project_options)

if(PLACEDB_BUILD_DEMO_SERVER OR PLACEDB_BUILD_ISOLATED_TESTS)
  # Ubuntu Noble's Drogon export unconditionally searches for every optional
  # database backend and uses a pre-CMake-4 JsonCpp finder. The demo consumes
  # only Drogon's HTTP API, so model the installed shared library narrowly.
  find_package(Threads REQUIRED)
  find_path(PLACEDB_DROGON_INCLUDE_DIR NAMES drogon/drogon.h REQUIRED)
  find_path(
    PLACEDB_JSONCPP_INCLUDE_DIR
    NAMES json/json.h
    PATH_SUFFIXES jsoncpp
    REQUIRED
  )
  find_library(PLACEDB_DROGON_LIBRARY NAMES drogon REQUIRED)
  find_library(PLACEDB_JSONCPP_LIBRARY NAMES jsoncpp REQUIRED)
  find_library(PLACEDB_TRANTOR_LIBRARY NAMES trantor REQUIRED)

  add_library(placedb_system_drogon UNKNOWN IMPORTED)
  set_target_properties(
    placedb_system_drogon
    PROPERTIES
      IMPORTED_LOCATION "${PLACEDB_DROGON_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES
        "${PLACEDB_DROGON_INCLUDE_DIR};${PLACEDB_JSONCPP_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES
        "${PLACEDB_TRANTOR_LIBRARY};${PLACEDB_JSONCPP_LIBRARY};Threads::Threads"
  )
  add_library(Drogon::Drogon ALIAS placedb_system_drogon)

endif()

if(PLACEDB_BUILD_DEMO_SERVER)
  add_executable(
    placedb_demo_server
    api/src/demo_main.cc
    api/src/http/demo_server.cc
  )
  target_include_directories(
    placedb_demo_server
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_demo_server
    PRIVATE
      Drogon::Drogon
      PlacementDB::ProjectOptions
  )
endif()

if(BUILD_TESTING AND PLACEDB_BUILD_ISOLATED_TESTS)
  add_executable(
    placedb_ranking_tests
    api/src/ranking/ranking.cc
    tests/drogon_test_main.cc
    tests/ranking/ranking_test.cc
  )
  target_include_directories(
    placedb_ranking_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_ranking_tests
    PRIVATE
      Drogon::Drogon
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_ranking_tests COMMAND placedb_ranking_tests)
endif()
