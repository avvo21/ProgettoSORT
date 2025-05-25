cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O3 -Wall -pthread")

# Libreria statica precompilata
add_library(rt_pthread STATIC rt/rt_pthread.cpp)
set_target_properties(rt_pthread PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/rt/librt_pthread.a")

# Eseguibili
add_executable(application_1 application_1.cpp executive.cpp busy_wait.cpp)
target_link_libraries(application_1 rt_pthread pthread rt)

add_executable(application_2 application_2.cpp executive.cpp busy_wait.cpp)
target_link_libraries(application_2 rt_pthread pthread rt)