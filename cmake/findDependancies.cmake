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
    elseif(${NO_CMAKE_SYSTEM_PATH})
        list(APPEND ARGS NO_CMAKE_SYSTEM_PATH)
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
    elseif(${VOLE_PSI_NO_SYSTEM_PATH})
        list(APPEND ARGS NO_DEFAULT_PATH PATHS ${CMAKE_PREFIX_PATH})
    endif()
    
    set(libOTe_options silentot silent_vole circuits)
    if(VOLE_PSI_ENABLE_SSE)
        set(libOTe_options ${libOTe_options} sse)
    else()
        set(libOTe_options ${libOTe_options} no_sse)
    endif()

    if(VOLE_PSI_ENABLE_BOOST)
        set(libOTe_options ${libOTe_options} boost)
    endif()
    if(VOLE_PSI_ENABLE_OPENSSL)
        set(libOTe_options ${libOTe_options} openssl)
    endif()

    if(VOLE_PSI_ENABLE_BITPOLYMUL)
        set(libOTe_options ${libOTe_options} bitpolymul)
    endif()
    
    if(VOLE_PSI_ENABLE_ASAN)
        set(libOTe_options ${libOTe_options} asan)
    else()
        set(libOTe_options ${libOTe_options} no_asan)
    endif()

    if(VOLE_PSI_ENABLE_PIC)
        set(libOTe_options ${libOTe_options} pic)
    else()
        set(libOTe_options ${libOTe_options} no_pic)
    endif()
    
    if(VOLE_PSI_ENABLE_SODIUM)
        set(libOTe_options ${libOTe_options} sodium)
        if(VOLE_PSI_SODIUM_MONTGOMERY)
            set(libOTe_options ${libOTe_options} sodium_montgomery)
        else()
            set(libOTe_options ${libOTe_options} no_sodium_montgomery)
        endif()
    endif()
        
    if(VOLE_PSI_ENABLE_RELIC)
        set(libOTe_options ${libOTe_options} relic)
    endif()
    message("\n\n\nlibOTe_options=${libOTe_options}")
    find_package(libOTe ${ARGS} COMPONENTS  ${libOTe_options})

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

# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
