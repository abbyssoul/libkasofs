# Build examples

# Example of using VFS as ram disk
set(EXAMPLE_APPFRAMEWORK_SOURCE_FILES ram_vfs.cpp)
add_executable(ram_vfs ${EXAMPLE_APPFRAMEWORK_SOURCE_FILES})
target_link_libraries(ram_vfs PUBLIC ${PROJECT_NAME} ${CONAN_LIBS})


add_custom_target(examples
    DEPENDS ram_vfs)
