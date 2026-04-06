cmake_minimum_required(VERSION 3.25)

project(MaccyWindows VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MACCY_BUILD_TESTS "Build unit tests" ON)

if(NOT WIN32)
  message(STATUS "Non-Windows host detected. Building portable core/test targets only.")
endif()

add_subdirectory(src)

if(MACCY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
