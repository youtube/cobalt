cmake_minimum_required(VERSION 3.6)

project (iamf2bear)

SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ")
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)


set(CODEC_LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(CODEC_INCLUDE_DIR  "${CMAKE_INSTALL_PREFIX}/include")


include_directories(
    ${CODEC_INCLUDE_DIR}
)

link_directories (
    ${CODEC_LIB_DIR}
)

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED iamf2bear.cpp)
  target_link_libraries (${PROJECT_NAME} bear ear)
endif()



set(IAMF2RESONANCE_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/iamf_bear_api.h)
set_target_properties(iamf2bear PROPERTIES PUBLIC_HEADER "${IAMF2RESONANCE_PUBLIC_HEADER}")
