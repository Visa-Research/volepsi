cmake_minimum_required (VERSION 3.15)

project(volePSI VERSION 0.1.2)

set(VOLEPSI_BUILD ON)

# sets some variables
include(cmake/preamble.cmake)

# sets build option
include(cmake/buildOptions.cmake)

# find all the dependancies.
include(cmake/findDependancies.cmake)

# build volePSI
add_subdirectory(volePSI)

# build the tests.
add_subdirectory(tests)

# link the executable.
add_subdirectory(frontend)

# setup the install
include(cmake/install.cmake)