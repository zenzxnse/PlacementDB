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
option(PLACEDB_BUILD_PRODUCTION_SERVER "Build the production Drogon API" ON)
option(PLACEDB_BUILD_FIXTURE_SERVER "Build the database-free fixture server" OFF)
option(PLACEDB_BUILD_ISOLATED_TESTS "Build isolated project test targets" ON)

if(PLACEDB_ENABLE_ASAN AND PLACEDB_ENABLE_UBSAN)
  message(FATAL_ERROR "ASan and UBSan must use separate presets")
endif()

add_library(placedb_project_options INTERFACE)
add_library(PlacementDB::ProjectOptions ALIAS placedb_project_options)

target_compile_features(placedb_project_options INTERFACE cxx_std_23)
placedb_enable_warnings(placedb_project_options)
placedb_enable_sanitizers(placedb_project_options)

if(PLACEDB_BUILD_PRODUCTION_SERVER OR PLACEDB_BUILD_FIXTURE_SERVER OR PLACEDB_BUILD_ISOLATED_TESTS)
  # Ubuntu Noble's Drogon export unconditionally searches for every optional
  # database backend and uses a pre-CMake-4 JsonCpp finder. Model the installed
  # Drogon shared library and its direct dependencies narrowly.
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

if(PLACEDB_BUILD_PRODUCTION_SERVER)
  find_library(
    PLACEDB_SODIUM_LIBRARY
    NAMES sodium libsodium.so.23
    REQUIRED
  )
  add_executable(
    placedb_server
    api/src/main.cc
    api/src/auth/authorization.cc
    api/src/auth/csrf.cc
    api/src/auth/ip_address.cc
    api/src/auth/password_executor.cc
    api/src/auth/rate_limiter.cc
    api/src/auth/secret.cc
    api/src/auth/session.cc
    api/src/config/server_config.cc
    api/src/db/import.cc
    api/src/db/experience_read_model.cc
    api/src/db/outbox.cc
    api/src/db/question_read_model.cc
    api/src/db/repository.cc
    api/src/dto/serialization.cc
    api/src/health/health_handlers.cc
    api/src/http/api_error.cc
    api/src/http/auth_routes.cc
    api/src/http/content_routes.cc
    api/src/http/question_routes.cc
    api/src/http/public_response_cache.cc
    api/src/http/request_context.cc
    api/src/http/request_policy.cc
    api/src/http/social_routes.cc
    api/src/ranking/ranking.cc
    api/src/search/search_worker.cc
    api/src/search/meilisearch_index.cc
    api/src/storage/avatar_store.cc
    api/src/validation/validator.cc
  )
  target_include_directories(
    placedb_server
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_server
    PRIVATE
      Drogon::Drogon
      "${PLACEDB_SODIUM_LIBRARY}"
      PlacementDB::ProjectOptions
  )
endif()

if(PLACEDB_BUILD_FIXTURE_SERVER)
  add_executable(
    placedb_fixture_server
    api/src/fixture_main.cc
    api/src/http/fixture_server.cc
  )
  target_include_directories(
    placedb_fixture_server
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_fixture_server
    PRIVATE
      Drogon::Drogon
      PlacementDB::ProjectOptions
  )
endif()

if(BUILD_TESTING AND PLACEDB_BUILD_ISOLATED_TESTS)
  add_executable(
    placedb_ranking_tests
    api/src/ranking/ranking.cc
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

  add_executable(
    placedb_api_contract_tests
    api/src/auth/ip_address.cc
    api/src/auth/session.cc
    api/src/http/api_error.cc
    api/src/validation/validator.cc
    tests/auth/session_test.cc
    tests/http/api_contract_test.cc
  )
  target_include_directories(
    placedb_api_contract_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_api_contract_tests
    PRIVATE
      Drogon::Drogon
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_api_contract_tests COMMAND placedb_api_contract_tests)

  add_executable(
    placedb_security_tests
    api/src/auth/authorization.cc
    api/src/auth/csrf.cc
    api/src/auth/rate_limiter.cc
    api/src/auth/secret.cc
    api/src/dto/serialization.cc
    api/src/http/request_policy.cc
    tests/security/security_contract_test.cc
  )
  target_include_directories(
    placedb_security_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_security_tests
    PRIVATE
      Drogon::Drogon
      "${PLACEDB_SODIUM_LIBRARY}"
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_security_tests COMMAND placedb_security_tests)

  add_executable(
    placedb_search_tests
    api/src/db/outbox.cc
    api/src/search/search_worker.cc
    tests/search/search_worker_test.cc
  )
  target_include_directories(
    placedb_search_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_search_tests
    PRIVATE
      Drogon::Drogon
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_search_tests COMMAND placedb_search_tests)

  add_executable(
    placedb_cache_tests
    api/src/http/public_response_cache.cc
    tests/http/public_response_cache_test.cc
  )
  target_include_directories(
    placedb_cache_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_cache_tests
    PRIVATE
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_cache_tests COMMAND placedb_cache_tests)

  add_executable(
    placedb_avatar_store_tests
    api/src/auth/secret.cc
    api/src/storage/avatar_store.cc
    tests/storage/avatar_store_test.cc
  )
  target_include_directories(
    placedb_avatar_store_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
  )
  target_link_libraries(
    placedb_avatar_store_tests
    PRIVATE
      Drogon::Drogon
      "${PLACEDB_SODIUM_LIBRARY}"
      PlacementDB::ProjectOptions
  )
  add_test(NAME placedb_avatar_store_tests COMMAND placedb_avatar_store_tests)

  # Sanitizer presets use an optimized configuration that defines NDEBUG.
  # These small contract binaries intentionally use assert(), so keep their
  # checks active in every build configuration.
  foreach(placedb_test_target IN ITEMS
      placedb_ranking_tests
      placedb_api_contract_tests
      placedb_security_tests
      placedb_search_tests
      placedb_cache_tests
      placedb_avatar_store_tests)
    target_compile_options(${placedb_test_target} PRIVATE -UNDEBUG)
  endforeach()
endif()
