cmake_minimum_required(VERSION 3.10)
project(antiafk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(antiafk main.cpp)

target_include_directories(antiafk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    target_link_libraries(antiafk user32 kernel32 imm32)
    
    if(MSVC)
        set_property(TARGET antiafk PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
