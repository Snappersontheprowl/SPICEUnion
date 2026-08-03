cmake_minimum_required(VERSION 3.20)

project(
  SPICEUnion
  VERSION 0.1.0
  DESCRIPTION "C++ execution core for interactive SPICE-class simulator workflows"
  LANGUAGES CXX
)

option(SPICEUNION_BUILD_TESTS "Build SPICEUnion tests" ON)
option(SPICEUNION_ENABLE_EXTERNAL_TESTS "Enable tests that require external EDA tools" OFF)

add_library(spiceunion_core STATIC
  src/core/evaluator.cpp
  src/core/task_result.cpp
  src/parse/result.cpp
  src/parse/result_reader.cpp
  src/pool/simulator_pool.cpp
  src/session/spectre_protocol.cpp
  src/session/spectre_session.cpp
  src/core/version.cpp
)

target_compile_features(spiceunion_core PUBLIC cxx_std_17)

target_include_directories(spiceunion_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(spiceunion_core PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(spiceunion_core PUBLIC stdc++fs)
endif()

if(SPICEUNION_BUILD_TESTS)
  enable_testing()

  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(spiceunion_smoke_test
    tests/smoke_test.cpp
  )

  target_link_libraries(spiceunion_smoke_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(spiceunion_smoke_test)

  add_executable(spiceunion_evaluator_contract_test
    tests/evaluator_contract_test.cpp
  )

  target_link_libraries(spiceunion_evaluator_contract_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_evaluator_contract_test)

  add_executable(spiceunion_result_test
    tests/result_test.cpp
  )

  target_link_libraries(spiceunion_result_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_result_test)

  add_executable(spiceunion_result_reader_test
    tests/result_reader_test.cpp
  )

  target_link_libraries(spiceunion_result_reader_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_result_reader_test)

  add_executable(spiceunion_spectre_session_lifecycle_test
    tests/spectre_session_lifecycle_test.cpp
  )

  target_link_libraries(spiceunion_spectre_session_lifecycle_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  target_compile_definitions(spiceunion_spectre_session_lifecycle_test
    PRIVATE
      SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
  )

  gtest_discover_tests(spiceunion_spectre_session_lifecycle_test)

  add_executable(spiceunion_spectre_protocol_test
    tests/spectre_protocol_test.cpp
  )

  target_link_libraries(spiceunion_spectre_protocol_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_spectre_protocol_test)
endif()
