cmake_minimum_required(VERSION 3.8)

project(main)

set(CMAKE_CXX_STANDARD 23)

set(MN_BUILD_EXEC OFF)
add_subdirectory(extern/midnight-graphics)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE midnight-graphics simple-lua)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/src)