find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
set(SPICEUNION_PYTHON_BINDING_EXECUTABLE "${Python3_EXECUTABLE}")
find_package(pybind11 CONFIG QUIET)

if(NOT pybind11_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    pybind11
    URL https://github.com/pybind/pybind11/archive/refs/tags/v2.13.6.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(spiceunion
  spiceunion_py.cpp
)

target_link_libraries(spiceunion
  PRIVATE
    spiceunion_core
)

target_compile_features(spiceunion PRIVATE cxx_std_17)

target_compile_definitions(spiceunion
  PRIVATE
    SPICEUNION_ENABLE_LIBPSF_READER_FOR_PYTHON=$<BOOL:${SPICEUNION_ENABLE_LIBPSF_READER}>
)

if(SPICEUNION_BUILD_TESTS)
  add_test(
    NAME python_import_smoke
    COMMAND ${SPICEUNION_PYTHON_BINDING_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_import.py
  )

  set_tests_properties(python_import_smoke
    PROPERTIES
      ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spiceunion>"
  )

  add_test(
    NAME python_api_contract_smoke
    COMMAND ${SPICEUNION_PYTHON_BINDING_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_api_contract.py
  )

  set_tests_properties(python_api_contract_smoke
    PROPERTIES
      ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spiceunion>"
  )

  add_test(
    NAME python_read_results_smoke
    COMMAND ${SPICEUNION_PYTHON_BINDING_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_read_results.py
            ${PROJECT_SOURCE_DIR}/tests/fixtures
  )

  set_tests_properties(python_read_results_smoke
    PROPERTIES
      ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spiceunion>"
  )

  add_test(
    NAME python_example_read_fixture_results
    COMMAND ${SPICEUNION_PYTHON_BINDING_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/read_fixture_results.py
            ${PROJECT_SOURCE_DIR}/tests/fixtures
  )

  set_tests_properties(python_example_read_fixture_results
    PROPERTIES
      ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:spiceunion>"
  )
endif()
