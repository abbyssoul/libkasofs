# Build examples
# TODO(abbyssoul): Disabled as not supported by clang (without gold linker that is)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -fwhole-program")


# Example of using VFS as ram disk
set(EXAMPLE_APPFRAMEWORK_SOURCE_FILES ram_vfs.cpp)
add_executable(ram_vfs ${EXAMPLE_APPFRAMEWORK_SOURCE_FILES})
target_link_libraries(ram_vfs PUBLIC ${PROJECT_NAME} ${CONAN_LIBS})


add_custom_target(examples
    DEPENDS ram_vfs)
