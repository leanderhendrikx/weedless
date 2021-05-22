# Sample dylib.
add_library(value SHARED value.c) 

# Sample executable.
add_executable(example_target example.c)
target_link_libraries(example_target value)

# Hook dylib.
add_library(hooks SHARED hooks.c) 
