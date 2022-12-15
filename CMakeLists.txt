# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(FolderDeTempla CXX RC)

# Unicode
add_definitions(-DUNICODE -D_UNICODE)

##############################################################################

# FolderDeTempla.exe
add_executable(FolderDeTempla WIN32
    FolderDeTempla.cpp
    CDropTarget.cpp
    CDropSource.cpp
    FolderDeTempla_res.rc)
target_link_libraries(FolderDeTempla comctl32 shlwapi)

##############################################################################
