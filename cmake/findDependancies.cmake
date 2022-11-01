include(${CMAKE_CURRENT_LIST_DIR}/preamble.cmake)

message(STATUS "VOLEPSI_THIRDPARTY_DIR=${VOLEPSI_THIRDPARTY_DIR}")


set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${VOLEPSI_THIRDPARTY_DIR};${CMAKE_PREFIX_PATH}")


#######################################
# sparsehash

macro(FIND_SPARSEHASH)
    set(ARGS ${ARGN})

    #explicitly asked to fetch sparsehash
    if(FETCH_SPARSEHASH)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${VOLEPSI_THIRDPARTY_DIR})
    endif()
    
    find_path(SPARSEHASH_INCLUDE_DIRS "sparsehash/dense_hash_map" PATH_SUFFIXES "include" ${ARGS})

    if(EXISTS ${SPARSEHASH_INCLUDE_DIRS})
        set(SPARSEHASH_FOUND ON)
    else()
        set(SPARSEHASH_FOUND OFF)
    endif()
endmacro()

if(FETCH_SPARSEHASH_AUTO)
    FIND_SPARSEHASH(QUIET)
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getSparsehash.cmake)
endif()

FIND_SPARSEHASH(REQUIRED)
message("SPARSEHASH_INCLUDE_DIRS=${SPARSEHASH_INCLUDE_DIRS}")

if(NOT TARGET sparsehash)
    add_library(sparsehash INTERFACE IMPORTED)
    
    target_include_directories(sparsehash INTERFACE 
                    $<BUILD_INTERFACE:${SPARSEHASH_INCLUDE_DIRS}>
                    $<INSTALL_INTERFACE:>)
endif()

#######################################
# libOTe

macro(FIND_LIBOTE)
    set(ARGS ${ARGN})

    #explicitly asked to fetch libOTe
    if(FETCH_LIBOTE)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${VOLEPSI_THIRDPARTY_DIR})
    endif()
    
    find_package(libOTe ${ARGS})

    if(TARGET oc::libOTe)
        set(libOTe_FOUND ON)
    else()
        set(libOTe_FOUND  OFF)
    endif()
endmacro()

if(FETCH_LIBOTE_AUTO)
    FIND_LIBOTE(QUIET)
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getLibOTe.cmake)
endif()

FIND_LIBOTE(REQUIRED)



#######################################
# libDivide

macro(FIND_LIBDIVIDE)
    set(ARGS ${ARGN})

    #explicitly asked to fetch libdivide
    if(FETCH_LIBDIVIDE)
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${VOLEPSI_THIRDPARTY_DIR})
    endif()

    find_path(LIBDIVIDE_INCLUDE_DIRS "libdivide.h" PATH_SUFFIXES "include" ${ARGS})
    if(EXISTS "${LIBDIVIDE_INCLUDE_DIRS}/libdivide.h")
        set(LIBDIVIDE_FOUND ON)
    else()
        set(LIBDIVIDE_FOUND OFF)
    endif()

endmacro()

if(FETCH_LIBDIVIDE_AUTO)
    FIND_LIBDIVIDE(QUIET)
    include(${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getLibDivide.cmake)
endif()

FIND_LIBDIVIDE(REQUIRED)

add_library(libdivide INTERFACE IMPORTED)
    
target_include_directories(libdivide INTERFACE 
                $<BUILD_INTERFACE:${LIBDIVIDE_INCLUDE_DIRS}>
                $<INSTALL_INTERFACE:>)

message(STATUS "LIBDIVIDE_INCLUDE_DIRS:  ${LIBDIVIDE_INCLUDE_DIRS}")


# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
