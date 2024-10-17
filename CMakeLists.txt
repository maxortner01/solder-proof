cmake_minimum_required(VERSION 3.8)

project(main)

set(CMAKE_CXX_STANDARD 23)

set(MN_BUILD_EXEC OFF)
add_subdirectory(extern/midnight-graphics)
add_subdirectory(extern/flecs)
add_subdirectory(extern/assimp)
add_subdirectory(extern/meshoptimizer)

add_executable(main 
    main.cpp 
    ${CMAKE_SOURCE_DIR}/src/Engine/Systems/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/Systems/Material.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/Model.cpp
    ${CMAKE_SOURCE_DIR}/src/Engine/Application.cpp
    ${CMAKE_SOURCE_DIR}/src/Game/MainScene.cpp
    ${CMAKE_SOURCE_DIR}/src/Util/Profiler.cpp)
    
target_link_libraries(main PRIVATE midnight-graphics simple-lua flecs assimp::assimp meshoptimizer)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(main PRIVATE -DRES_DIR="${CMAKE_SOURCE_DIR}/res")