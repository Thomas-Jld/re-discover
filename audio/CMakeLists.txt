cmake_minimum_required(VERSION 3.10)
# set the project name
project(MAIN)


# add the executable
add_executable(TEST ${VENDORS_HEADERS} main.cpp)

target_link_libraries(TEST pthread )
message(STATUS ${Boost_INCLUDE_DIRS} )


# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)


file(GLOB_RECURSE VENDORS_HEADERS vendors/*.hpp)

# Include the vendor headers
include_directories(
    ${CMAKE_SOURCE_DIR}/vendors
    ${CMAKE_SOURCE_DIR}/vendors/fmod/core
    ${CMAKE_SOURCE_DIR}/vendors/websocketpp
)

target_link_libraries(
    TEST
    ${CMAKE_SOURCE_DIR}/vendors/fmod/lib/x86_64/libfmod.so
    ${CMAKE_SOURCE_DIR}/vendors/fmod/lib/x86_64/libfmodL.so

)
