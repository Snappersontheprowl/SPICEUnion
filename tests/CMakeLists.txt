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

include(GoogleTest)

function(spiceunion_add_gtest target)
  add_executable(${target}
    ${ARGN}
  )

  target_include_directories(${target}
    PRIVATE
      ${PROJECT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(${target}
    PRIVATE
      spiceunion_core
      GTest::gtest_main
  )

  gtest_discover_tests(${target})
endfunction()

function(spiceunion_add_ocp_gtest target)
  add_executable(${target}
    ${ARGN}
  )

  target_include_directories(${target}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_link_libraries(${target}
    PRIVATE
      ocp::ordered_concurrent_pool
      GTest::gtest_main
  )

  gtest_discover_tests(${target})
endfunction()

spiceunion_add_gtest(spiceunion_smoke_test
  unit/core/smoke_test.cpp
)

spiceunion_add_gtest(spiceunion_toolchain_test
  unit/toolchain/toolchain_test.cpp
)

spiceunion_add_gtest(spiceunion_evaluator_contract_test
  unit/core/evaluator_contract_test.cpp
)

target_compile_definitions(spiceunion_evaluator_contract_test
  PRIVATE
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

spiceunion_add_ocp_gtest(spiceunion_ordered_concurrent_pool_test
  unit/pool/ordered_concurrent_pool_test.cpp
)

spiceunion_add_gtest(spiceunion_simulator_pool_contract_test
  unit/pool/simulator_pool_contract_test.cpp
)

target_compile_definitions(spiceunion_simulator_pool_contract_test
  PRIVATE
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

spiceunion_add_gtest(spiceunion_result_test
  unit/core/result_test.cpp
)

spiceunion_add_gtest(spiceunion_result_reader_test
  unit/parse/result_reader_test.cpp
)

target_compile_definitions(spiceunion_result_reader_test
  PRIVATE
    SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
    SPICEUNION_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures"
)

spiceunion_add_gtest(spiceunion_psf_ascii_reader_test
  unit/parse/psf_ascii_reader_test.cpp
)

target_compile_definitions(spiceunion_psf_ascii_reader_test
  PRIVATE
    SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
    SPICEUNION_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures"
)

spiceunion_add_gtest(spiceunion_simulation_semantics_test
  integration/semantics/simulation_semantics_test.cpp
)

target_compile_definitions(spiceunion_simulation_semantics_test
  PRIVATE
    SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
    SPICEUNION_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures"
)

spiceunion_add_gtest(spiceunion_workflow_test
  unit/workflow/workflow_test.cpp
)

target_compile_definitions(spiceunion_workflow_test
  PRIVATE
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

spiceunion_add_gtest(spiceunion_spectre_protocol_test
  unit/session/spectre_protocol_test.cpp
)

spiceunion_add_gtest(spiceunion_ngspice_config_test
  unit/session/ngspice_config_test.cpp
)

spiceunion_add_gtest(spiceunion_ngspice_netlist_test
  unit/session/ngspice_netlist_test.cpp
)

spiceunion_add_gtest(spiceunion_ngspice_wrdata_parser_test
  unit/session/ngspice_wrdata_parser_test.cpp
)

target_compile_definitions(spiceunion_ngspice_wrdata_parser_test
  PRIVATE
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

spiceunion_add_gtest(spiceunion_spectre_session_lifecycle_test
  external/spectre/spectre_session_lifecycle_test.cpp
)

target_compile_definitions(spiceunion_spectre_session_lifecycle_test
  PRIVATE
    SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
    SPICEUNION_SPECTRE_MATERIALS_DIR="${SPICEUNION_SPECTRE_MATERIALS_DIR}"
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

if(SPICEUNION_ENABLE_LIBPSF_READER)
  spiceunion_add_gtest(spiceunion_spectre_capability_test
    external/spectre/spectre_capability_test.cpp
  )

  target_compile_definitions(spiceunion_spectre_capability_test
    PRIVATE
      SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
      SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
      SPICEUNION_SPECTRE_MATERIALS_DIR="${SPICEUNION_SPECTRE_MATERIALS_DIR}"
      SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
  )

  spiceunion_add_gtest(spiceunion_spectre_real_result_test
    external/spectre/spectre_real_result_test.cpp
  )

  target_compile_definitions(spiceunion_spectre_real_result_test
    PRIVATE
      SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
      SPICEUNION_ENABLE_LIBPSF_READER=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
      SPICEUNION_SPECTRE_MATERIALS_DIR="${SPICEUNION_SPECTRE_MATERIALS_DIR}"
      SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
  )
endif()

spiceunion_add_gtest(spiceunion_ngspice_session_external_test
  external/ngspice/ngspice_session_external_test.cpp
)

target_compile_definitions(spiceunion_ngspice_session_external_test
  PRIVATE
    SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

spiceunion_add_gtest(spiceunion_ngspice_evaluator_external_test
  external/ngspice/ngspice_evaluator_external_test.cpp
)

target_compile_definitions(spiceunion_ngspice_evaluator_external_test
  PRIVATE
    SPICEUNION_ENABLE_EXTERNAL_TESTS=$<BOOL:${SPICEUNION_ENABLE_EXTERNAL_TESTS}>
    SPICEUNION_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)
