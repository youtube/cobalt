cmake_minimum_required(VERSION 3.6)

project (iamf2resonance)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11") 
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)

set(EXTERNAL_LIB_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/resonance_audio/lib")
set(EXTERNAL_INCLUDE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/resonance_audio/include")

include_directories(
    ${EXTERNAL_INCLUDE_DIR}
)

link_directories (
    ${EXTERNAL_LIB_DIR}
)

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED iamf2resonance.cc)
  target_link_libraries (${PROJECT_NAME} ResonanceAudioStatic)
endif()



set(IAMF2RESONANCE_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/iamf_resonance_api.h)
set_target_properties(iamf2resonance PROPERTIES PUBLIC_HEADER "${IAMF2RESONANCE_PUBLIC_HEADER}")

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/binaural
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/resonance)