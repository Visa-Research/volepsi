

#############################################
#            Install                        #
#############################################


configure_file("${CMAKE_CURRENT_LIST_DIR}/findDependancies.cmake" "findDependancies.cmake" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/preamble.cmake" "preamble.cmake" COPYONLY)

# make cache variables for install destinations
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


# generate the config file that is includes the exports
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/volePSIConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volePSI
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

if(NOT DEFINED volePSI_VERSION_MAJOR)
    message("\n\n\n\n warning, volePSI_VERSION_MAJOR not defined ${volePSI_VERSION_MAJOR}")
endif()

set_property(TARGET volePSI PROPERTY VERSION ${volePSI_VERSION})

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/volePSIConfigVersion.cmake"
  VERSION "${volePSI_VERSION_MAJOR}.${volePSI_VERSION_MINOR}.${volePSI_VERSION_PATCH}"
  COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/volePSIConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/volePSIConfigVersion.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/findDependancies.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/preamble.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volePSI
)

# install library
install(
    TARGETS volePSI
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    EXPORT volePSITargets)

# install headers
install(
    DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../volePSI"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/"
    FILES_MATCHING PATTERN "*.h")


# install config
install(EXPORT volePSITargets
  FILE volePSITargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volePSI
       NAMESPACE visa::
)
 export(EXPORT volePSITargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/volePSITargets.cmake"
       NAMESPACE visa::
)