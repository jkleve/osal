cmake_minimum_required(VERSION 3.16)

project(osal)
option(OSAL_TEST "Build tests" ON)

add_library(osal STATIC src/os.cpp)
add_library(osal::osal ALIAS osal)
set_target_properties(osal PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(osal PUBLIC include)
target_include_directories(osal PRIVATE src)

if (OSAL_TEST)
    message(STATUS "Building tests")

    # Adjust cmake paths to look in build folder for Find*.cmake files
    set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
    set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

    find_package(GTest REQUIRED)

    enable_testing()
    add_subdirectory(test)
endif ()
