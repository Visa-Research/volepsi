set(DEP_NAME            sparsehash-c11)
set(GIT_REPOSITORY      https://github.com/sparsehash/sparsehash-c11.git)
set(GIT_TAG             "edd6f1180156e76facc1c0449da245208ab39503" )

set(CLONE_DIR "${CMAKE_CURRENT_LIST_DIR}/${DEP_NAME}")
set(BUILD_DIR "${CLONE_DIR}/build/${VOLEPSI_CONFIG}")
set(LOG_FILE  "${CMAKE_CURRENT_LIST_DIR}/log-${DEP_NAME}.txt")


include("${CMAKE_CURRENT_LIST_DIR}/fetch.cmake")

if(NOT SPARSEHASH_FOUND)
    find_program(GIT git REQUIRED)
    set(DOWNLOAD_CMD  ${GIT} clone ${GIT_REPOSITORY})
    set(CHECKOUT_CMD  ${GIT} checkout ${GIT_TAG})
    
    message("============= Building ${DEP_NAME} =============")
    if(NOT EXISTS ${CLONE_DIR})
        run(NAME "Cloning ${GIT_REPOSITORY}" CMD ${DOWNLOAD_CMD} WD ${CMAKE_CURRENT_LIST_DIR})
    endif()

    run(NAME "Checkout ${GIT_TAG} " CMD ${CHECKOUT_CMD}  WD ${CLONE_DIR})
    file(COPY ${CLONE_DIR}/sparsehash DESTINATION ${VOLEPSI_THIRDPARTY_DIR}/include/)
    #if(MSVC)
    #    message("Install")
    #    file(COPY ${CLONE_DIR}/src/windows/sparsehash DESTINATION ${VOLEPSI_THIRDPARTY_DIR}/include/)
    #else()
    #    file(COPY ${CLONE_DIR}/src/sparsehash DESTINATION ${VOLEPSI_THIRDPARTY_DIR}/include/)
    #    #run("Configure" CMD ./configure --prefix=${VOLEPSI_THIRDPARTY_DIR} WD ${CLONE_DIR})
    #    #run("make" CMD make -j ${PARALLEL_FETCH} WD ${CLONE_DIR})
    #    #run("install" CMD make install WD ${CLONE_DIR})
    #endif()
    message("log ${LOG_FILE}\n==========================================")
else()
    message("${DEP_NAME} already fetched.")
endif()


install(CODE "message(\"sparsehash begin ------------------\")")    
install(
    DIRECTORY "${CLONE_DIR}/sparsehash"
    DESTINATION "include")

install(CODE "message(\"sparsehash end ------------------\")")    

        
#install(CODE "
#    if(MSVC)
#        file(COPY ${CLONE_DIR}/src/sparsehash DESTINATION \${CMAKE_INSTALL_PREFIX}/include/)
#        file(COPY ${CLONE_DIR}/src/windows/sparsehash DESTINATION \${CMAKE_INSTALL_PREFIX}/include/)
#    else()
#        message(\"sparse hash install\")
#        execute_process(
#            COMMAND ./configure --prefix=
#            COMMAND make -j ${PARALLEL_FETCH}
#            COMMAND ${SUDO} make DESTDIR= install
#            WORKING_DIRECTORY ${CLONE_DIR}
#            RESULT_VARIABLE RESULT
#            COMMAND_ECHO STDOUT
#        )
#        message(\"sparse hash install done\")
#
#    endif()
#")