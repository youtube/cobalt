cmake_minimum_required(VERSION 3.6)

project (iamf2resonance)

SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O3")
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)

set(CODEC_LIB_DIR  "${PROJECT_SOURCE_DIR}/resonance_audio/lib")
set(CODEC_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/resonance_audio/include")

include_directories(
    ${CODEC_INCLUDE_DIR}
)

link_directories (
    ${CODEC_LIB_DIR}
)

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED resonance_audio_api2.cc)
  target_link_libraries (${PROJECT_NAME} ResonanceAudioStatic)
endif()



set(IAMF2RESONANCE_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/resonance_audio_api2.h)
set_target_properties(iamf2resonance PROPERTIES PUBLIC_HEADER "${IAMF2RESONANCE_PUBLIC_HEADER}")