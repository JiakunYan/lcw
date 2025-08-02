function(add_lcw_executable name)
  add_executable(${name} ${ARGN})
  set_target_properties(
    ${name}
    PROPERTIES CXX_STANDARD 17
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  target_compile_definitions(${name} PRIVATE _GNU_SOURCE)
  target_link_libraries(${name} PRIVATE lcw)
  set_target_properties(${name} PROPERTIES OUTPUT_NAME "lcw_${name}")
  install(TARGETS ${name} RUNTIME)
endfunction()

function(add_lcw_test name)
  cmake_parse_arguments(ARG "" "" "COMMANDS;LABELS;ENVIRONMENT" ${ARGN})

  list(LENGTH ARG_COMMANDS count)
  set(index 0)
  while(index LESS count)
    set(test_name test-${ARG_LABELS}-${name})
    if(index GREATER 0)
      set(test_name test-${ARG_LABELS}-${name}-${index})
    endif()
    list(GET ARG_COMMANDS ${index} COMMAND)
    math(EXPR index "${index}+1")
    # set test
    string(REGEX REPLACE "\\[TARGET\\]" $<TARGET_FILE:${name}> TEST_COMMAND
                         ${COMMAND})
    string(REPLACE " " ";" TEST_COMMAND ${TEST_COMMAND})
    add_test(NAME ${test_name} COMMAND ${TEST_COMMAND})
    set_property(TEST ${test_name} PROPERTY LABELS ${ARG_LABELS})
    if(ENVIRONMENT)
      set_tests_properties(${test_name} PROPERTIES ENVIRONMENT ${ENVIRONMENT})
    endif()
  endwhile()
endfunction()
