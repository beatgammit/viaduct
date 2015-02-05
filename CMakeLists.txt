cmake_minimum_required(VERSION 2.8.11)
project(viaduct)
enable_testing()

add_executable(example examples/main.c)
target_link_libraries(example viaduct)

set(CMAKE_C_FLAGS "-std=c99 -fPIC")

add_executable(tests test/main.c)
target_link_libraries(tests viaduct)
target_link_libraries(tests tap)
include_directories("${PROJECT_SOURCE_DIR}")

add_test(NAME unit_tests COMMAND tests)

add_custom_command(TARGET tests POST_BUILD_COMMAND tests)

option(USE_MSGPACK "Use msgpack serialization" ON) 
option(DEBUG "Include debugging print lines" ON) 
if(USE_MSGPACK)
	add_definitions(-DUSE_MSGPACK)
endif()
if(DEBUG)
	add_definitions(-DDEBUG)
endif()

add_library(viaduct SHARED viaduct.c)

add_library(cmp vendor/cmp.c)

if(USE_MSGPACK)
  target_link_libraries(viaduct cmp)
  include_directories("${PROJECT_SOURCE_DIR}/cmp")
endif(USE_MSGPACK)
