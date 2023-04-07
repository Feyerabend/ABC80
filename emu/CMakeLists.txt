cmake_minimum_required(VERSION 3.9)

project (abc)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_C_STANDARD_REQUIRED 11)
set(CMAKE_C_FLAGS "-std=c11 -Wall -o2")

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR}/src)

add_executable (abc src/abc.c src/z80.c)

set_property(TARGET abc PROPERTY C_STANDARD 11)

target_link_libraries(abc ${CURSES_LIBRARIES})