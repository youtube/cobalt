cmake_minimum_required(VERSION 3.6)

project (iamf)
message(STATUS "start build iamf")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)

add_definitions(-DIAMF_RIMARY_PROFILE=001)
add_definitions(-DIAMF_ADDITIONAL_PROFILE=001)

set(CODEC_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/lib")
set(CODEC_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/dep_codecs/include")
set(EXTER_LIB_DIR  "${PROJECT_SOURCE_DIR}/dep_external/lib")
set(EXTER_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/dep_external/include")

option(SUPPORT_VERIFIER "Output vlogging file" OFF)
if (SUPPORT_VERIFIER)
  message(STATUS "Output vlogging file")
  add_definitions(-DSUPPORT_VERIFIER=1)
endif()

option(CODEC_CAP "Codec capability check" ON)
option(BINAURALIZER "Enable binaural rendering" OFF)

if(BINAURALIZER)
  message(STATUS "Enable binaural rendering")
  add_definitions(-DDISABLE_BINAURALIZER=0)
endif()

if(CODEC_CAP)
  unset(find_opus CACHE)
  find_library(find_opus NAMES opus PATHS ${CODEC_LIB_DIR} NO_DEFAULT_PATH)
  if(${find_opus} STREQUAL "find_opus-NOTFOUND")
      message(WARNING "the opus library was not found")
  else()
      add_definitions(-DCONFIG_OPUS_CODEC)
  endif()

  unset(find_aac CACHE)
  find_library(find_aac NAMES fdk-aac PATHS ${CODEC_LIB_DIR} NO_DEFAULT_PATH)
  if(${find_aac} STREQUAL "find_aac-NOTFOUND")
      message(WARNING "the fdk-aac library was not found")
  else()
      add_definitions(-DCONFIG_AAC_CODEC)
  endif()

  unset(find_flac CACHE)
  find_library(find_flac NAMES FLAC PATHS ${CODEC_LIB_DIR} NO_DEFAULT_PATH)
  if(${find_flac} STREQUAL "find_flac-NOTFOUND")
      message(WARNING "the FLAC library was not found")
  else()
      add_definitions(-DCONFIG_FLAC_CODEC)
  endif()
endif()

include_directories (include)
aux_source_directory (dep_external/src/wav DIR_DEP_EXTERNAL_WAV)
aux_source_directory (src/common DIR_IAMF_COMMON)
aux_source_directory (src/iamf_dec DIR_IAMF_DEC)
if(NOT ${find_opus} STREQUAL "find_opus-NOTFOUND")
  aux_source_directory (src/iamf_dec/opus DIR_IAMF_DEC_OPUS)
endif()
if(NOT ${find_aac} STREQUAL "find_aac-NOTFOUND")
  aux_source_directory (src/iamf_dec/aac DIR_IAMF_DEC_AAC)
endif()
if(NOT ${find_flac} STREQUAL "find_flac-NOTFOUND")
  aux_source_directory (src/iamf_dec/flac DIR_IAMF_DEC_FLAC)
endif()
aux_source_directory (src/iamf_dec/pcm DIR_IAMF_DEC_PCM)



include_directories(
    ${CODEC_INCLUDE_DIR}
    ${EXTER_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/src/common
    ${PROJECT_SOURCE_DIR}/src/iamf_dec
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/opus
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/aac
    ${PROJECT_SOURCE_DIR}/src/iamf_dec/flac
)
link_directories (
    ${CODEC_LIB_DIR}
)

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED ${DIR_DEP_EXTERNAL_WAV} ${DIR_IAMF_COMMON}
    ${DIR_IAMF_DEC_OPUS} ${DIR_IAMF_DEC_AAC} ${DIR_IAMF_DEC_FLAC} ${DIR_IAMF_DEC_PCM} ${DIR_IAMF_DEC})

  if(NOT ${find_opus} STREQUAL "find_opus-NOTFOUND")
    target_link_libraries (${PROJECT_NAME} opus)
  endif()

  if(NOT ${find_aac} STREQUAL "find_aac-NOTFOUND")
    target_link_libraries (${PROJECT_NAME} fdk-aac)
  endif()

  if(NOT ${find_flac} STREQUAL "find_flac-NOTFOUND")
    target_link_libraries (${PROJECT_NAME} FLAC)
  endif()

else()
  add_library(${PROJECT_NAME} STATIC ${DIR_DEP_EXTERNAL_WAV} ${DIR_IAMF_COMMON}
    ${DIR_IAMF_DEC_OPUS} ${DIR_IAMF_DEC_AAC} ${DIR_IAMF_DEC_PCM}
    ${DIR_IAMF_DEC_FLAC} ${DIR_IAMF_DEC})
endif()



set(IAMF_PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IAMF_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IAMF_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vlogging_tool_sr.h
    )
set_target_properties(iamf PROPERTIES PUBLIC_HEADER "${IAMF_PUBLIC_HEADER}")


set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(iamf.pc.in iamf.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iamf.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

if(BUILD_SHARED_LIBS)
  install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
else()
  install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/iamf)
endif()

if(BINAURALIZER)
  install(DIRECTORY ${EXTER_LIB_DIR}/binaural
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

