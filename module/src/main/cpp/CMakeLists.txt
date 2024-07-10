cmake_minimum_required(VERSION 3.22.1)
project(sample)

set(LINKER_FLAGS "-ffixed-x18 -Wl,--hash-style=both")

# TODO: set visibility only for our libs (-fvisibility=hidden -fvisibility-inlines-hidden)
set(CXX_FLAGS "${CXX_FLAGS} -fno-exceptions -fno-rtti")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

add_subdirectory(external)
link_libraries(cxx)

# libutils stub
add_library(utils SHARED binder/stub_utils.cpp)
target_include_directories(utils PUBLIC binder/include)

# libbinder stub
add_library(binder SHARED binder/stub_binder.cpp)
target_include_directories(binder PUBLIC binder/include)
target_link_libraries(binder utils)

add_library(${MODULE_NAME} SHARED example.cpp)
target_link_libraries(${MODULE_NAME} log binder utils)
