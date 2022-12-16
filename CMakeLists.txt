# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(f-templa CXX RC)

# Unicode
add_definitions(-DUNICODE -D_UNICODE)

# set output directory (build/)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

##############################################################################

# libtempla and templa.exe
add_subdirectory(templa)

# f-templa.exe
add_executable(f-templa WIN32
    f-templa.cpp
    CDropTarget.cpp
    CDropSource.cpp
    f-templa_res.rc)
target_link_libraries(f-templa comctl32 shlwapi libtempla)

##############################################################################
