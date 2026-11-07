cmake_minimum_required(VERSION 4.3)
project(Ur)

set(CMAKE_CXX_STANDARD 26)

# raylib
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 6.0
            GIT_SHALLOW 1
    )
    FetchContent_MakeAvailable(raylib)
endif()

# raylib-cpp
find_package(raylib_cpp QUIET)
if (NOT raylib_cpp_FOUND)
    if (NOT DEFINED RAYLIB_CPP_VERSION)
        set(RAYLIB_CPP_VERSION master)
    endif()
    include(FetchContent)
    FetchContent_Declare(
            raylib_cpp
            GIT_REPOSITORY https://github.com/RobLoach/raylib-cpp.git
            GIT_TAG ${RAYLIB_CPP_VERSION}
    )
    FetchContent_MakeAvailable(raylib_cpp)
endif()

# Lua
FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(sol2)
find_package(Lua REQUIRED)

add_executable(${PROJECT_NAME} main.cpp
        json.hpp
        lua_convert.hpp
        lua_convert.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC raylib PUBLIC raylib_cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE sol2 ${LUA_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
