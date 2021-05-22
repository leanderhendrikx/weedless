cmake_minimum_required(VERSION 3.17)
project(weedless)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor")

add_subdirectory(weedless)
add_subdirectory(example)

