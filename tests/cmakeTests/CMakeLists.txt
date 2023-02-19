cmake_minimum_required(VERSION 3.15)
project(cmakeTests)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()

find_package(volePSI REQUIRED HINTS ${VOLEPSI_HINT})

add_executable(main "main.cpp")

if(MSVC)
    set_target_properties(main
        PROPERTIES
            CXX_STANDARD ${VOLE_PSI_STD_VER}
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
else()
	target_compile_options(main PRIVATE
		"-std=c++${VOLE_PSI_STD_VER}"
	)    
endif()

target_link_libraries(main visa::volePSI)