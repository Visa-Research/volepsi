

#############################################
#               Build volePSI_Tests            #
#############################################


set(SRCS
	"Paxos_Tests.cpp"
	"RsOprf_Tests.cpp"
	"RsPsi_Tests.cpp"
	"UnitTests.cpp"
    "FileBase_Tests.cpp"
	)
	
if(VOLE_PSI_ENABLE_GMW)
    list(APPEND SRCS
        "GMW_Tests.cpp"
    )
endif()

if(VOLE_PSI_ENABLE_CPSI)
    list(APPEND SRCS
        "RsCpsi_Tests.cpp"
    )
endif()
if(VOLE_PSI_ENABLE_OPPRF)
    list(APPEND SRCS
        "RsOpprf_Tests.cpp"
    )
endif()

add_library(volePSI_Tests ${SRCS})

target_link_libraries(volePSI_Tests volePSI)
