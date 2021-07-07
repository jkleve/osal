add_executable(test_osal test_osal.cpp)
set_target_properties(test_osal PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(test_osal PUBLIC osal::osal GTest::gtest_main)
