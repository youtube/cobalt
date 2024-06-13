cmake_minimum_required(VERSION 3.6)

project (iamfplayer)

message(status,"+++++++++++iamfplayer+++++++++++++")

option(MULTICHANNEL_BINAURALIZER "Enable multichannel binaural rendering" OFF)
option(HOA_BINAURALIZER "Enable HOA binaural rendering" OFF)
option(SUPPORT_VERIFIER "Output vlogging file" OFF)

include_directories (include)
aux_source_directory (player DIR_IAMFPLAY_PLAYER)
aux_source_directory (src DIR_IAMFPLAY_SRC)

set(LIB_DIR  "${CMAKE_INSTALL_PREFIX}/lib")
set(INCLUDE_IAMF_DIR  "${CMAKE_INSTALL_PREFIX}/include/iamf")
set(INCLUDE_DEP_EXTERNAL_WAV_DIR  "../../../dep_external/include/wav")
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
else()
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--no-as-need -Werror=unused-variable")
endif()

if(MULTICHANNEL_BINAURALIZER)
  message(STATUS "Enable multichannel binaural rendering")
  add_definitions(-DENABLE_MULTICHANNEL_TO_BINAURAL=1)
endif()

if(HOA_BINAURALIZER)
  message(STATUS "Enable HOA binaural rendering")
  add_definitions(-DENABLE_HOA_TO_BINAURAL=1)
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

if(MULTICHANNEL_BINAURALIZER OR HOA_BINAURALIZER)
link_directories(
  ${LIB_DIR}/binaural
  ${LIB_DIR}
)
else()
link_directories(
  ${LIB_DIR}
)
endif()

add_executable (iamfplayer player/iamfplayer.c
    ${DIR_IAMFPLAY_PLAYER} ${DIR_IAMFPLAY_SRC})

if(MULTICHANNEL_BINAURALIZER OR HOA_BINAURALIZER)
  if(MULTICHANNEL_BINAURALIZER)
    target_link_libraries (iamfplayer iamf m iamf2bear boost_filesystem boost_system boost_thread efl objectmodel panning pml rbbl rcl rrl visr)
  endif()
  if(HOA_BINAURALIZER)
    target_link_libraries (iamfplayer iamf m iamf2resonance)
  endif()
else()
  target_link_libraries (iamfplayer iamf m)
endif()

