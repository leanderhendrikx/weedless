file(GLOB_RECURSE WEEDLESS_SRC "lib/*.cpp")

set(WEEDLESS_INCLUDE ${CMAKE_SOURCE_DIR}/weedless/include)

add_library(weedless-core STATIC ${WEEDLESS_SRC})
target_include_directories(weedless-core PUBLIC ${WEEDLESS_INCLUDE})

add_executable(weedless weedless.cpp)
target_link_libraries(weedless weedless-core)
target_include_directories(weedless PUBLIC ${WEEDLESS_INCLUDE})

