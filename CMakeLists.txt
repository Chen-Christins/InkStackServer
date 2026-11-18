cmake_minimum_required(VERSION 3.22)
project(InkStackServer)

include(cmake/common.cmake)
include(cmake/utils.cmake)
include(cmake/dependencies.cmake)

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/protocol/include)

add_subdirectory(src)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/module)
