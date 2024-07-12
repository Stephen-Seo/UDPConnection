cmake_minimum_required(VERSION 3.15)
project(PackageTest CXX)

find_package(udpc CONFIG REQUIRED)

add_executable(example src/test.cpp)
target_link_libraries(example udpc::udpc)
