cmake_minimum_required(VERSION 3.8)

project(solder-proof)

set(CMAKE_CXX_STANDARD 20)

set(MN_BUILD_EXEC OFF)
add_subdirectory(extern/midnight-graphics)
add_subdirectory(extern/flecs)
add_subdirectory(extern/assimp)
add_subdirectory(extern/meshoptimizer)

add_library(solder-proof
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Systems/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Systems/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Util/Profiler.cpp)
    
target_link_libraries(solder-proof PUBLIC midnight-graphics simple-lua flecs assimp::assimp meshoptimizer)
target_include_directories(solder-proof PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(solder-proof PRIVATE -DRES_DIR="${CMAKE_SOURCE_DIR}/res")