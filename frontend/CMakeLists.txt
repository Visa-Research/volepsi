
file(GLOB_RECURSE SRCS *.cpp)

add_executable(frontend  ${SRCS})

target_link_libraries(frontend volePSI_Tests)


