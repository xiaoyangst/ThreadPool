cmake_minimum_required(VERSION 3.16)
project(SyncThreadPool)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(. SRC)

add_executable(SyncThreadPool main.cpp ${SRC})
target_link_libraries(SyncThreadPool pthread)