set(TEST_SOURCE_FILES
        ci/teamcity_messages.cpp
        ci/teamcity_gtest.cpp

        main_gtest.cpp

        test_permissions.cpp
        test_inode.cpp
        test_vfs.cpp
    )


enable_testing()

add_executable(test_${PROJECT_NAME} EXCLUDE_FROM_ALL ${TEST_SOURCE_FILES})

target_link_libraries(test_${PROJECT_NAME}
    ${PROJECT_NAME}
    $<$<NOT:$<PLATFORM_ID:Darwin>>:rt>
    )

add_test(NAME test_${PROJECT_NAME}
    COMMAND test_${PROJECT_NAME}
    )
