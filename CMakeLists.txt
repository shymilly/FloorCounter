cmake_minimum_required(VERSION 3.10)
project(FacadeParser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(facade_parser facade_parser.cpp)

target_link_libraries(facade_parser ${OpenCV_LIBS})
target_include_directories(facade_parser PRIVATE ${OpenCV_INCLUDE_DIRS})

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
