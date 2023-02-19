

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")

	############################################
	#          If top level cmake              #
	############################################
	if(MSVC)
		# optionally add the following to CMAKE_PREFIX_PATH
		#if(NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED NO_OC_DEFAULT_PREFIX)
		#	set(CMAKE_PREFIX_PATH 
		#		"c:/libs"
		#		"${CMAKE_CURRENT_SOURCE_DIR}/..;"
		#		)
		#endif()
		
	else()
		set(COMMON_FLAGS "-Wall -Wfatal-errors")

		if(NOT DEFINED NO_ARCH_NATIVE)
			set(COMMON_FLAGS "${COMMON_FLAGS} -march=native")
		endif()

		SET(CMAKE_CXX_FLAGS_RELEASE "-O3  -DNDEBUG")
		SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb")
		SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb")
		#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++17")
		
	endif()



	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")
	

	############################################
	#           Build mode checks              #
	############################################

	# Set a default build type for single-configuration
	# CMake generators if no build type is set.
	if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	   SET(CMAKE_BUILD_TYPE Release)
	endif()

	if(    NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug"
       AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo" )

        message(WARNING ": Unknown build type - \${CMAKE_BUILD_TYPE}=${CMAKE_BUILD_TYPE}.  Please use one of Debug, Release, or RelWithDebInfo. e.g. call\n\tcmake . -DCMAKE_BUILD_TYPE=Release\n" )
	endif()
endif()

if(MSVC)
    set(VOLEPSI_CONFIG_NAME "${CMAKE_BUILD_TYPE}")
    if("${VOLEPSI_CONFIG_NAME}" STREQUAL "RelWithDebInfo" OR "${VOLEPSI_CONFIG_NAME}" STREQUAL "")
        set(VOLEPSI_CONFIG_NAME "Release")
	endif()
    set(VOLEPSI_CONFIG "x64-${VOLEPSI_CONFIG_NAME}")
elseif(APPLE)
    set(VOLEPSI_CONFIG "osx")
else()
    set(VOLEPSI_CONFIG "linux")
endif()

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/install.cmake)
	set(VOLEPSI_IN_BUILD_TREE ON)
else()
	set(VOLEPSI_IN_BUILD_TREE OFF)
endif()

# we are in the build tree. We might be building the library or 
# someone is consuming us from the build tree.
if(VOLEPSI_IN_BUILD_TREE)

	set(VOLEPSI_BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/../out/build/${VOLEPSI_CONFIG}")
	get_filename_component(VOLEPSI_BUILD_DIR ${VOLEPSI_BUILD_DIR} ABSOLUTE)

	# true if we are actually building
	if(VOLEPSI_BUILD)
		# warn if we aren't using out/build/<config>/
		if(NOT (${CMAKE_BINARY_DIR} STREQUAL ${VOLEPSI_BUILD_DIR}))
			message(WARNING "unexpected build directory. \n\tCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}\nbut expect\n\tVOLEPSI_BUILD_DIR=${VOLEPSI_BUILD_DIR}")
		endif()
		set(VOLEPSI_BUILD_DIR ${CMAKE_BINARY_DIR})
	else()
	endif()


	if(NOT DEFINED VOLEPSI_THIRDPARTY_DIR)
		set(VOLEPSI_THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../out/install/${VOLEPSI_CONFIG}")
		get_filename_component(VOLEPSI_THIRDPARTY_DIR ${VOLEPSI_THIRDPARTY_DIR} ABSOLUTE)
	endif()
else()
    # we currenty are in install tree, <install-prefix>/lib/cmake/vole-psi
	if(NOT DEFINED VOLEPSI_THIRDPARTY_DIR)
		set(VOLEPSI_THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
		get_filename_component(VOLEPSI_THIRDPARTY_DIR ${VOLEPSI_THIRDPARTY_DIR} ABSOLUTE)
	endif()
endif()


if(NOT VOLE_PSI_THIRDPARTY_CLONE_DIR)
	get_filename_component(VOLE_PSI_THIRDPARTY_CLONE_DIR "${CMAKE_CURRENT_LIST_DIR}/../out/" ABSOLUTE)
endif()
