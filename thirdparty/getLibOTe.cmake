
set(USER_NAME           )      
set(TOKEN               )      
set(GIT_REPOSITORY      "https://github.com/osu-crypto/libOTe.git")
set(GIT_TAG             "747b4fd3dcbc7618e78dbafdf975a8fb617a6bdd" )

set(DEP_NAME            libOTe)          
set(CLONE_DIR "${VOLE_PSI_THIRDPARTY_CLONE_DIR}/${DEP_NAME}")
set(BUILD_DIR "${CLONE_DIR}/out/build/${VOLEPSI_CONFIG}")
set(LOG_FILE  "${CMAKE_CURRENT_LIST_DIR}/log-${DEP_NAME}.txt")


include("${CMAKE_CURRENT_LIST_DIR}/fetch.cmake")

option(LIBOTE_DEV "always build libOTe" OFF)

if(NOT ${DEP_NAME}_FOUND OR LIBOTE_DEV)

    string (REPLACE ";" "%" CMAKE_PREFIX_PATH_STR "${CMAKE_PREFIX_PATH}")

    find_program(GIT git REQUIRED)
    set(DOWNLOAD_CMD  ${GIT} clone --recursive ${GIT_REPOSITORY})
    set(CHECKOUT_CMD  ${GIT} checkout ${GIT_TAG})
    set(SUBMODULE_CMD   ${GIT} submodule update --recursive)
    set(CONFIGURE_CMD ${CMAKE_COMMAND} -S ${CLONE_DIR} -B ${BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                       -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_STR}
                       -DNO_SYSTEM_PATH=${VOLE_PSI_NO_SYSTEM_PATH}
                       -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} 
                       -DFETCH_AUTO=${FETCH_AUTO}
                       -DVERBOSE_FETCH=${VERBOSE_FETCH}
                       -DENABLE_CIRCUITS=ON
                       -DENABLE_MRR=ON
                       -DENABLE_IKNP=ON
                       -DENABLE_SOFTSPOKEN_OT=ON
                       -DENABLE_BITPOLYMUL=${VOLE_PSI_ENABLE_BITPOLYMUL}
                       -DENABLE_SILENTOT=ON
                       -DENABLE_SILENT_VOLE=ON
                       -DENABLE_SSE=${VOLE_PSI_ENABLE_SSE}
                       -DENABLE_BOOST=${VOLE_PSI_ENABLE_BOOST}
                       -DENABLE_OPENSSL=${VOLE_PSI_ENABLE_OPENSSL}
                       -DLIBOTE_STD_VER=${VOLE_PSI_STD_VER}
                       -DENABLE_PIC=${VOLE_PSI_ENABLE_PIC}
                       -DENABLE_ASAN=${VOLE_PSI_ENABLE_ASAN}
                       -DOC_THIRDPARTY_CLONE_DIR=${VOLE_PSI_THIRDPARTY_CLONE_DIR}
                       -DOC_THIRDPARTY_INSTALL_PREFIX=${VOLEPSI_THIRDPARTY_DIR}
                       -DENABLE_SODIUM=${VOLE_PSI_ENABLE_SODIUM}
                       -DENABLE_RELIC=${VOLE_PSI_ENABLE_RELIC}
                       -DSODIUM_MONTGOMERY=${VOLE_PSI_SODIUM_MONTGOMERY}
                       )
    set(BUILD_CMD     ${CMAKE_COMMAND} --build ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE})
    set(INSTALL_CMD   ${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE} --prefix ${VOLEPSI_THIRDPARTY_DIR})
    

    message("============= Building ${DEP_NAME} =============")
    if(NOT EXISTS ${CLONE_DIR})
        run(NAME "Cloning ${GIT_REPOSITORY}" CMD ${DOWNLOAD_CMD} WD ${VOLE_PSI_THIRDPARTY_CLONE_DIR})
    endif()


    run(NAME "libOTe Checkout ${GIT_TAG} " CMD ${CHECKOUT_CMD}  WD ${CLONE_DIR})
    run(NAME "libOTe submodule"       CMD ${SUBMODULE_CMD} WD ${CLONE_DIR})
    run(NAME "libOTe Configure"       CMD ${CONFIGURE_CMD} WD ${CLONE_DIR})
    run(NAME "libOTe Build"           CMD ${BUILD_CMD}     WD ${CLONE_DIR})
    run(NAME "libOTe Install"         CMD ${INSTALL_CMD}   WD ${CLONE_DIR})

    message("log ${LOG_FILE}\n==========================================")
else()
    message("${DEP_NAME} already fetched.")
endif()

install(CODE "
    if(NOT CMAKE_INSTALL_PREFIX STREQUAL \"${VOLE_PSI_THIRDPARTY_CLONE_DIR}\")
        execute_process(
            COMMAND ${SUDO} \${CMAKE_COMMAND} --install ${BUILD_DIR}  --config ${CMAKE_BUILD_TYPE} --prefix \${CMAKE_INSTALL_PREFIX}
            WORKING_DIRECTORY ${CLONE_DIR}
            RESULT_VARIABLE RESULT
            COMMAND_ECHO STDOUT
        )
    endif()
")
