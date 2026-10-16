# Drop any .cpp file here: it is built automatically as build/<preset>/bin/play_<name>.
file(GLOB PLAY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(src ${PLAY_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(play_${name} ${src})
  target_link_libraries(play_${name} PRIVATE lab_common)
  set_target_properties(play_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endforeach()
