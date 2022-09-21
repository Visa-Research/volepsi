cmake_minimum_required(VERSION 3.15)
project(cmakeTests)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()

find_package(volePSI REQUIRED HINTS ${VOLEPSI_HINT})

add_executable(main "main.cpp")
target_link_libraries(main visa::volePSI)