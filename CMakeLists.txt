cmake_minimum_required(VERSION 2.8.11)
project(viaduct)
enable_testing()

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/cmp")

add_executable(example examples/main.c)
target_link_libraries(example viaduct)

set(CMAKE_C_FLAGS "-fPIC")

add_executable(tests test/main.c)
target_link_libraries(tests viaduct)
target_link_libraries(tests tap)

add_test(NAME unit_tests COMMAND tests)

add_custom_command(TARGET tests POST_BUILD_COMMAND tests)

option(DEBUG "Include debugging print lines" ON) 
if(DEBUG)
	add_definitions(-DDEBUG)
endif()

add_library(viaduct SHARED viaduct.c)
target_link_libraries(viaduct cmp)

add_library(cmp vendor/cmp.c)
