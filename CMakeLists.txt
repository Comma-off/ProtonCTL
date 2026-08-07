cmake_minimum_required(VERSION 3.20)
project(protonctl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(PROTONCTL_BUILD_TESTS "Build unit tests for the C++ core" OFF)

add_subdirectory(backend)
