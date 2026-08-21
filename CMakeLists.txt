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
option(PLACEDB_ENABLE_TSAN "Enable ThreadSanitizer for project targets" OFF)
option(PLACEDB_BUILD_PRODUCTION_SERVER "Build the production Drogon API" ON)
option(PLACEDB_BUILD_FIXTURE_SERVER "Build the database-free fixture server" OFF)
option(PLACEDB_BUILD_ISOLATED_TESTS "Build isolated project test targets" ON)

set(placedb_enabled_sanitizers 0)
foreach(placedb_sanitizer_flag IN ITEMS
    PLACEDB_ENABLE_ASAN PLACEDB_ENABLE_UBSAN PLACEDB_ENABLE_TSAN)
  if(${placedb_sanitizer_flag})
    math(EXPR placedb_enabled_sanitizers "${placedb_enabled_sanitizers} + 1")
  endif()
endforeach()
if(placedb_enabled_sanitizers GREATER 1)
  message(FATAL_ERROR "Sanitizers are mutually exclusive; use separate presets")
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

if(PLACEDB_BUILD_PRODUCTION_SERVER OR
   (BUILD_TESTING AND PLACEDB_BUILD_ISOLATED_TESTS))
  find_package(ICU 74 REQUIRED COMPONENTS uc)
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
    api/src/app/runtime.cc
    api/src/app/request_executor.cc
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
    api/src/db/content_lookup.cc
    api/src/db/outbox.cc
    api/src/db/question_read_model.cc
    api/src/db/repository.cc
    api/src/dto/serialization.cc
    api/src/health/health_handlers.cc
    api/src/http/api_error.cc
    api/src/http/auth_routes.cc
    api/src/http/content_routes.cc
    api/src/http/current_user.cc
    api/src/http/question_routes.cc
    api/src/http/public_response_cache.cc
    api/src/http/request_context.cc
    api/src/http/request_policy.cc
    api/src/http/social_routes.cc
    api/src/http/moderation_routes.cc
    api/src/http/submission_contract.cc
    api/src/http/submission_routes.cc
    api/src/ranking/ranking.cc
    api/src/ranking/difficulty_score.cc
    api/src/search/search_worker.cc
    api/src/search/meilisearch_index.cc
    api/src/storage/avatar_store.cc
    api/src/validation/validator.cc
    api/src/validation/unicode_text.cc
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
      ICU::uc
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
  # Declares one isolated contract test binary. Every target automatically
  # receives the project options, the api/include path, CTest registration,
  # and -UNDEBUG. Sanitizer presets use an optimized configuration that
  # defines NDEBUG, and these binaries intentionally check with assert(), so
  # their assertions must stay active in every build configuration. Using
  # this helper is what keeps that guarantee; do not add a test target with
  # a bare add_executable.
  function(placedb_add_test target_name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LINK")
    if(NOT arg_SOURCES)
      message(FATAL_ERROR "placedb_add_test(${target_name}) needs SOURCES")
    endif()
    add_executable("${target_name}" ${arg_SOURCES})
    target_include_directories(
      "${target_name}"
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
    )
    target_link_libraries(
      "${target_name}"
      PRIVATE
        ${arg_LINK}
        PlacementDB::ProjectOptions
    )
    target_compile_options("${target_name}" PRIVATE -UNDEBUG)
    add_test(NAME "${target_name}" COMMAND "${target_name}")
  endfunction()

  placedb_add_test(placedb_ranking_tests
    SOURCES
      api/src/ranking/difficulty_score.cc
      api/src/ranking/ranking.cc
      tests/ranking/ranking_test.cc
    LINK Drogon::Drogon
  )

  placedb_add_test(placedb_api_contract_tests
    SOURCES
      api/src/auth/ip_address.cc
      api/src/auth/session.cc
      api/src/db/content_lookup.cc
      api/src/http/api_error.cc
      api/src/http/submission_contract.cc
      api/src/validation/validator.cc
      api/src/validation/unicode_text.cc
      tests/auth/session_test.cc
      tests/http/api_contract_test.cc
      tests/http/content_lookup_test.cc
      tests/http/submission_contract_test.cc
    LINK Drogon::Drogon ICU::uc
  )

  placedb_add_test(placedb_security_tests
    SOURCES
      api/src/auth/authorization.cc
      api/src/auth/csrf.cc
      api/src/auth/rate_limiter.cc
      api/src/auth/secret.cc
      api/src/dto/serialization.cc
      api/src/http/request_policy.cc
      tests/security/security_contract_test.cc
    LINK Drogon::Drogon "${PLACEDB_SODIUM_LIBRARY}"
  )

  placedb_add_test(placedb_search_tests
    SOURCES
      api/src/db/outbox.cc
      api/src/search/meilisearch_index.cc
      api/src/search/search_worker.cc
      tests/search/search_worker_test.cc
    LINK Drogon::Drogon
  )

  placedb_add_test(placedb_cache_tests
    SOURCES
      api/src/http/public_response_cache.cc
      tests/http/public_response_cache_test.cc
  )

  placedb_add_test(placedb_avatar_store_tests
    SOURCES
      api/src/auth/secret.cc
      api/src/storage/avatar_store.cc
      tests/storage/avatar_store_test.cc
    LINK Drogon::Drogon "${PLACEDB_SODIUM_LIBRARY}"
  )

  placedb_add_test(placedb_server_config_tests
    SOURCES
      api/src/config/server_config.cc
      tests/app/server_config_test.cc
  )

  placedb_add_test(placedb_request_executor_tests
    SOURCES
      api/src/app/request_executor.cc
      tests/app/request_executor_test.cc
  )

  placedb_add_test(placedb_ip_address_tests
    SOURCES
      api/src/auth/ip_address.cc
      tests/auth/ip_address_test.cc
  )

  placedb_add_test(placedb_repository_contract_tests
    SOURCES
      tests/db/repository_test.cc
    LINK Drogon::Drogon
  )

  placedb_add_test(placedb_moderation_contract_tests
    SOURCES
      tests/http/moderation_contract_test.cc
  )
  target_compile_definitions(placedb_moderation_contract_tests
    PRIVATE PLACEDB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  set(
    PLACEDB_WIRED_CPP_TEST_SOURCES
    tests/app/server_config_test.cc
    tests/app/request_executor_test.cc
    tests/auth/ip_address_test.cc
    tests/auth/session_test.cc
    tests/db/repository_test.cc
    tests/http/api_contract_test.cc
    tests/http/content_lookup_test.cc
    tests/http/submission_contract_test.cc
    tests/http/moderation_contract_test.cc
    tests/http/public_response_cache_test.cc
    tests/ranking/ranking_test.cc
    tests/search/search_worker_test.cc
    tests/security/security_contract_test.cc
    tests/storage/avatar_store_test.cc
  )
  file(
    GLOB_RECURSE PLACEDB_ALL_CPP_TEST_SOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "tests/*.cc"
  )
  foreach(placedb_test_source IN LISTS PLACEDB_ALL_CPP_TEST_SOURCES)
    if(NOT placedb_test_source IN_LIST PLACEDB_WIRED_CPP_TEST_SOURCES)
      message(
        FATAL_ERROR
        "C++ test source is not assigned to a target: ${placedb_test_source}"
      )
    endif()
  endforeach()
endif()
