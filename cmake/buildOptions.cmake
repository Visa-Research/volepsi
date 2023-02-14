
macro(EVAL var)
     if(${ARGN})
         set(${var} ON)
     else()
         set(${var} OFF)
     endif()
endmacro()

option(FETCH_AUTO      "automaticly download and build dependancies" OFF)
option(VERBOSE_FETCH    "Verbose fetch" ON)


if(DEFINED COPROTO_ENABLE_BOOST)
    message("warning, setting VOLE_PSI_ENABLE_BOOST as COPROTO_ENABLE_BOOST=${COPROTO_ENABLE_BOOST}")
    set(VOLE_PSI_ENABLE_BOOST ${COPROTO_ENABLE_BOOST})
    unset(COPROTO_ENABLE_BOOST CACHE)
endif()

if(DEFINED COPROTO_ENABLE_OPENSSL)
    message("warning, setting VOLE_PSI_ENABLE_OPENSSL as COPROTO_ENABLE_OPENSSL=${COPROTO_ENABLE_OPENSSL}")
    set(VOLE_PSI_ENABLE_OPENSSL ${COPROTO_ENABLE_OPENSSL})
    unset(COPROTO_ENABLE_OPENSSL CACHE)
endif()


if(DEFINED LIBOTE_ENABLE_BITPOLYMUL)
    message("warning, setting VOLE_PSI_ENABLE_BITPOLYMUL as LIBOTE_ENABLE_BITPOLYMUL=${LIBOTE_ENABLE_BITPOLYMUL}")
    set(VOLE_PSI_ENABLE_BITPOLYMUL ${LIBOTE_ENABLE_BITPOLYMUL})
    unset(LIBOTE_ENABLE_BITPOLYMUL CACHE)
endif()

#option(FETCH_SPARSEHASH		"download and build sparsehash" OFF))
EVAL(FETCH_SPARSEHASH_AUTO 
	(DEFINED FETCH_SPARSEHASH AND FETCH_SPARSEHASH) OR
	((NOT DEFINED FETCH_SPARSEHASH) AND (FETCH_AUTO)))

#option(FETCH_LIBOTE		"download and build libOTe" OFF))
EVAL(FETCH_LIBOTE_AUTO 
	(DEFINED FETCH_LIBOTE AND FETCH_LIBOTE) OR
	((NOT DEFINED FETCH_LIBOTE) AND (FETCH_AUTO)))

#option(FETCH_LIBDIVIDE		"download and build libdivide" OFF))
EVAL(FETCH_LIBDIVIDE_AUTO 
	(DEFINED FETCH_LIBDIVIDE AND FETCH_LIBDIVIDE) OR
	((NOT DEFINED FETCH_LIBDIVIDE) AND (FETCH_AUTO)))


message(STATUS "vole-psi options\n=======================================================")

message(STATUS "Option: FETCH_AUTO        = ${FETCH_AUTO}")
message(STATUS "Option: FETCH_SPARSEHASH  = ${FETCH_SPARSEHASH}")
message(STATUS "Option: FETCH_LIBOTE      = ${FETCH_LIBOTE}")
message(STATUS "Option: FETCH_LIBDIVIDE   = ${FETCH_LIBDIVIDE}")
message(STATUS "Option: VERBOSE_FETCH     = ${VERBOSE_FETCH}")


if(NOT DEFINED VOLE_PSI_STD_VER)
    set(VOLE_PSI_STD_VER 17)
endif()

option(VOLE_PSI_ENABLE_SSE    "build the library with SSE intrisics" ON)
option(VOLE_PSI_ENABLE_GMW    "compile the library with GMW" ON)
option(VOLE_PSI_ENABLE_CPSI   "compile the library with circuit PSI" ON)
option(VOLE_PSI_ENABLE_OPPRF  "compile the library with OPPRF" ON)
option(VOLE_PSI_ENABLE_BOOST   "build coproto with boost support" OFF)
option(VOLE_PSI_ENABLE_OPENSSL   "build coproto with boost openssl support" OFF)
option(VOLE_PSI_PIC           "build with PIC" OFF)
if(APPLE)
    option(VOLE_PSI_ENABLE_BITPOLYMUL   "build libOTe with quasiCyclic support" OFF)
else()
    option(VOLE_PSI_ENABLE_BITPOLYMUL   "build libOTe with quasiCyclic support" ON)
endif()


if(VOLE_PSI_ENABLE_CPSI)
    set(VOLE_PSI_ENABLE_GMW ${VOLE_PSI_ENABLE_CPSI})
endif()
if(VOLE_PSI_ENABLE_CPSI)
    set(VOLE_PSI_ENABLE_OPPRF ${VOLE_PSI_ENABLE_CPSI})
endif()



message("\n")
message(STATUS "Option: VOLE_PSI_ENABLE_SSE        = ${VOLE_PSI_ENABLE_SSE}")
message(STATUS "Option: VOLE_PSI_PIC               = ${VOLE_PSI_PIC}")
message(STATUS "Option: VOLE_PSI_STD_VER           = ${VOLE_PSI_STD_VER}")
                                                  
message(STATUS "Option: VOLE_PSI_ENABLE_GMW        = ${VOLE_PSI_ENABLE_GMW}")
message(STATUS "Option: VOLE_PSI_ENABLE_CPSI       = ${VOLE_PSI_ENABLE_CPSI}")
message(STATUS "Option: VOLE_PSI_ENABLE_OPPRF      = ${VOLE_PSI_ENABLE_OPPRF}\n")
                                                  
message(STATUS "Option: VOLE_PSI_ENABLE_BOOST      = ${VOLE_PSI_ENABLE_BOOST}")
message(STATUS "Option: VOLE_PSI_ENABLE_OPENSSL    = ${VOLE_PSI_ENABLE_OPENSSL}")
message(STATUS "Option: VOLE_PSI_ENABLE_BITPOLYMUL = ${VOLE_PSI_ENABLE_BITPOLYMUL}")


