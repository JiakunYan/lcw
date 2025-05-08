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
