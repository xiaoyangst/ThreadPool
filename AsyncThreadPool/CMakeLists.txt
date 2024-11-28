cmake_minimum_required(VERSION 3.16)
project(AsyncThreadPool)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(. SRC)

add_executable(AsyncThreadPool main.cpp ${SRC})
target_link_libraries(AsyncThreadPool pthread)