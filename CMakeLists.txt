cmake_minimum_required(VERSION 3.0)
project(peekaboo)
set(CMAKE_CXX_STANDARD 14)

include_directories(./third_party/json)

add_executable(peekaboo main.cc)
