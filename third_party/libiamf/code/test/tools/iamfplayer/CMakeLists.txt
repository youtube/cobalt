cmake_minimum_required(VERSION 3.6)

project (iamfplayer)

message(status,"+++++++++++iamfplayer+++++++++++++")

option(BINAURALIZER "Enable binaural rendering" OFF)
option(SUPPORT_VERIFIER "Output vlogging file" OFF)

include_directories (include)
aux_source_directory (player DIR_IAMFPLAY_PLAYER)
aux_source_directory (src DIR_IAMFPLAY_SRC)

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
set(INCLUDE_DEP_EXTERNAL_WAV_DIR  "../../../dep_external/include/wav")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--no-as-need")

if(BINAURALIZER)
  message(STATUS "Enable binaural rendering")
  add_definitions(-DDISABLE_BINAURALIZER=0)
endif()

if(SUPPORT_VERIFIER)
  message(STATUS "Output vlogging file")
  add_definitions(-DSUPPORT_VERIFIER=1)
endif()


message(status,${INCLUDE_IAMF_DIR})
include_directories(
    ${INCLUDE_IAMF_DIR}
    ${INCLUDE_DEP_EXTERNAL_WAV_DIR}
)
link_directories(
  ${LIB_DIR}/binaural
  ${LIB_DIR}
)


add_executable (iamfplayer player/iamfplayer.c
    ${DIR_IAMFPLAY_PLAYER} ${DIR_IAMFPLAY_SRC})

if(BINAURALIZER)
  target_link_libraries (iamfplayer iamf m iamf2bear iamf2resonance boost_filesystem boost_system boost_thread efl objectmodel panning pml rbbl rcl rrl visr)
else()
  target_link_libraries (iamfplayer iamf m)
endif()

