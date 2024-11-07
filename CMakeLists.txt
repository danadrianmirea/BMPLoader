cmake_minimum_required(VERSION 3.10) 
project(BMPLoaderWin32)

set(CMAKE_CXX_STANDARD 23)               
set(CMAKE_CXX_STANDARD_REQUIRED True)    

add_subdirectory(code)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()