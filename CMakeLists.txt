cmake_minimum_required(VERSION 3.20)

project(
  SPICEUnion
  VERSION 0.1.0
  DESCRIPTION "C++ execution core for interactive SPICE-class simulator workflows"
  LANGUAGES CXX
)

option(SPICEUNION_BUILD_TESTS "Build SPICEUnion tests" ON)
option(SPICEUNION_ENABLE_EXTERNAL_TESTS "Enable tests that require external EDA tools" OFF)
option(SPICEUNION_ENABLE_LIBPSF_READER "Enable optional henjo/libpsf PSF result reader backend" OFF)

set(SPICEUNION_LIBPSF_INCLUDE_DIR "" CACHE PATH "Directory that contains henjo/libpsf psf.h")
set(SPICEUNION_LIBPSF_LIBRARY "" CACHE FILEPATH "Path to henjo/libpsf library")
set(SPICEUNION_ORDERED_POOL_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../OrderedConcurrentPool"
    CACHE PATH
    "Path to the standalone OrderedConcurrentPool project"
)

if(NOT EXISTS "${SPICEUNION_ORDERED_POOL_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "SPICEUNION_ORDERED_POOL_SOURCE_DIR must point to the standalone "
    "OrderedConcurrentPool project. Current value: ${SPICEUNION_ORDERED_POOL_SOURCE_DIR}"
  )
endif()

set(ORDERED_CONCURRENT_POOL_BUILD_TESTS OFF CACHE BOOL
    "Build OrderedConcurrentPool tests when embedded by SPICEUnion" FORCE)
set(ORDERED_CONCURRENT_POOL_BUILD_EXAMPLES OFF CACHE BOOL
    "Build OrderedConcurrentPool examples when embedded by SPICEUnion" FORCE)
add_subdirectory(
  "${SPICEUNION_ORDERED_POOL_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/_deps/ordered_concurrent_pool"
)

if(SPICEUNION_ENABLE_LIBPSF_READER)
  if(SPICEUNION_LIBPSF_INCLUDE_DIR)
    set(_spiceunion_libpsf_include_dir "${SPICEUNION_LIBPSF_INCLUDE_DIR}")
  else()
    unset(_spiceunion_libpsf_include_dir)
    find_path(_spiceunion_libpsf_include_dir
      NAMES psf.h
      HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/local/external/libpsf/install/include"
    )
  endif()

  if(SPICEUNION_LIBPSF_LIBRARY)
    set(_spiceunion_libpsf_library "${SPICEUNION_LIBPSF_LIBRARY}")
  else()
    unset(_spiceunion_libpsf_library)
    find_library(_spiceunion_libpsf_library
      NAMES psf libpsf.a
      HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/local/external/libpsf/install/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/local/external/libpsf/install/lib64"
    )
  endif()

  if(NOT _spiceunion_libpsf_include_dir OR NOT _spiceunion_libpsf_library)
    message(FATAL_ERROR
      "SPICEUNION_ENABLE_LIBPSF_READER=ON requires henjo/libpsf. "
      "Set SPICEUNION_LIBPSF_INCLUDE_DIR and SPICEUNION_LIBPSF_LIBRARY, "
      "or build it under local/external/libpsf/install."
    )
  endif()

  set(SPICEUNION_LIBPSF_INCLUDE_DIR "${_spiceunion_libpsf_include_dir}" CACHE PATH
      "Directory that contains henjo/libpsf psf.h" FORCE)
  set(SPICEUNION_LIBPSF_LIBRARY "${_spiceunion_libpsf_library}" CACHE FILEPATH
      "Path to henjo/libpsf library" FORCE)
endif()

add_library(spiceunion_core STATIC
  src/core/evaluator.cpp
  src/core/task_result.cpp
  src/parse/result.cpp
  src/parse/result_reader.cpp
  src/pool/simulator_pool.cpp
  src/session/ngspice_session.cpp
  src/session/spectre_protocol.cpp
  src/session/spectre_session.cpp
  src/core/version.cpp
)

if(SPICEUNION_ENABLE_LIBPSF_READER)
  target_sources(spiceunion_core
    PRIVATE
      src/parse/libpsf_backend.cpp
  )
endif()

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

target_link_libraries(spiceunion_core
  PRIVATE
    ocp::ordered_concurrent_pool
)

if(SPICEUNION_ENABLE_LIBPSF_READER)
  target_compile_definitions(spiceunion_core
    PRIVATE
      SPICEUNION_ENABLE_LIBPSF_READER=1
  )

  target_include_directories(spiceunion_core SYSTEM
    PRIVATE
      ${SPICEUNION_LIBPSF_INCLUDE_DIR}
  )

  target_link_libraries(spiceunion_core
    PRIVATE
      ${SPICEUNION_LIBPSF_LIBRARY}
  )
endif()

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

  add_executable(spiceunion_ordered_concurrent_pool_test
    tests/ordered_concurrent_pool_test.cpp
  )

  target_link_libraries(spiceunion_ordered_concurrent_pool_test
    PRIVATE
      ocp::ordered_concurrent_pool
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_ordered_concurrent_pool_test)

  add_executable(spiceunion_simulator_pool_contract_test
    tests/simulator_pool_contract_test.cpp
  )

  target_include_directories(spiceunion_simulator_pool_contract_test
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(spiceunion_simulator_pool_contract_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(spiceunion_simulator_pool_contract_test)

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

  target_compile_definitions(spiceunion_result_reader_test
    PRIVATE
      SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
      SPICEUNION_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
  )

  gtest_discover_tests(spiceunion_result_reader_test)

  add_executable(spiceunion_simulation_semantics_test
    tests/simulation_semantics_test.cpp
  )

  target_link_libraries(spiceunion_simulation_semantics_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  target_compile_definitions(spiceunion_simulation_semantics_test
    PRIVATE
      SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
      SPICEUNION_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
  )

  gtest_discover_tests(spiceunion_simulation_semantics_test)

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

  add_executable(spiceunion_ngspice_session_test
    tests/ngspice_session_test.cpp
  )

  target_link_libraries(spiceunion_ngspice_session_test
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  target_compile_definitions(spiceunion_ngspice_session_test
    PRIVATE
      SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
  )

  gtest_discover_tests(spiceunion_ngspice_session_test)

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
