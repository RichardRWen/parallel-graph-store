cmake_minimum_required(VERSION 3.10)
project(ParallelDiskANN)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_PREFIX_PATH "parlaylib/lib")

find_package(Parlay REQUIRED)

include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
