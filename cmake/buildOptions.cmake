
macro(EVAL var)
     if(${ARGN})
         set(${var} ON)
     else()
         set(${var} OFF)
     endif()
endmacro()

option(FETCH_AUTO      "automaticly download and build dependancies" OFF)
option(VERBOSE_FETCH    "Verbose fetch" ON)


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



option(VOLE_PSI_ENABLE_SSE    "build the library with SSE intrisics" ON)
option(VOLE_PSI_ENABLE_GMW    "compile the library with GMW" ON)
option(VOLE_PSI_ENABLE_CPSI   "compile the library with circuit PSI" ON)
option(VOLE_PSI_ENABLE_OPPRF  "compile the library with OPPRF" ON)
option(COPROTO_ENABLE_BOOST   "build coproto with boost support" OFF)
option(COPROTO_ENABLE_OPENSSL   "build coproto with boost openssl support" OFF)

if(APPLE)
    option(LIBOTE_ENABLE_BITPOLYMUL   "build libOTe with quasiCyclic support" OFF)
else()
    option(LIBOTE_ENABLE_BITPOLYMUL   "build libOTe with quasiCyclic support" ON)
endif()


if(VOLE_PSI_ENABLE_CPSI)
    set(VOLE_PSI_ENABLE_GMW ${VOLE_PSI_ENABLE_CPSI})
endif()
if(VOLE_PSI_ENABLE_CPSI)
    set(VOLE_PSI_ENABLE_OPPRF ${VOLE_PSI_ENABLE_CPSI})
endif()



message("\n")
message(STATUS "Option: VOLE_PSI_ENABLE_SSE      = ${VOLE_PSI_ENABLE_SSE}")
message(STATUS "Option: VOLE_PSI_ENABLE_GMW      = ${VOLE_PSI_ENABLE_GMW}")
message(STATUS "Option: VOLE_PSI_ENABLE_CPSI     = ${VOLE_PSI_ENABLE_CPSI}")
message(STATUS "Option: VOLE_PSI_ENABLE_OPPRF    = ${VOLE_PSI_ENABLE_OPPRF}\n")

message(STATUS "Option: COPROTO_ENABLE_BOOST     = ${COPROTO_ENABLE_BOOST}\n")
message(STATUS "Option: COPROTO_ENABLE_OPENSSL   = ${COPROTO_ENABLE_OPENSSL}\n")
message(STATUS "Option: LIBOTE_ENABLE_BITPOLYMUL = ${LIBOTE_ENABLE_BITPOLYMUL}\n")


set(VOLE_PSI_CPP_VER 17)