# lab_demo(<name>
#     [SOURCES a.cpp b.cpp]   # defaults to <name>.cpp
#     [RACY]                  # contains an intentional data race: skipped by ctest under TSan
#     [UNSAFE]                # intentionally dangerous: never run by ctest
#     [TEST_ARGS ...])        # shortened arguments so ctest stays fast
#
# Executables land in <build>/bin so scripts/lab.sh can discover them.
function(lab_demo name)
  cmake_parse_arguments(ARG "RACY;UNSAFE" "" "SOURCES;TEST_ARGS" ${ARGN})
  if(NOT ARG_SOURCES)
    set(ARG_SOURCES ${name}.cpp)
  endif()
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE lab_common)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

  if(LAB_STAGE AND TARGET stage${LAB_STAGE})
    add_dependencies(stage${LAB_STAGE} ${name})
  endif()

  set(_run TRUE)
  if(ARG_UNSAFE)
    set(_run FALSE)
  endif()
  if(ARG_RACY AND LAB_SANITIZE STREQUAL "thread")
    set(_run FALSE)
  endif()
  if(_run)
    add_test(NAME ${name} COMMAND ${name} ${ARG_TEST_ARGS})
    set_tests_properties(${name} PROPERTIES LABELS "stage${LAB_STAGE}" TIMEOUT 180
      ENVIRONMENT "NO_COLOR=1;TSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tsan.supp:halt_on_error=0")
  endif()
endfunction()

# lab_stage(NN) creates the aggregate target `stageNN` ("cmake --build . --target stage04").
macro(lab_stage num)
  set(LAB_STAGE ${num})
  add_custom_target(stage${num})
endmacro()
