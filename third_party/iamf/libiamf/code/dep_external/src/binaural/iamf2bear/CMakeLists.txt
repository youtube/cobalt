cmake_minimum_required(VERSION 3.6)

project (iamf2bear)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11") 
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SHARED_LIBS "Build shared library" ON)

set(EXTERNAL_LIB_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(EXTERNAL_INCLUDE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(
    ${EXTERNAL_INCLUDE_DIR}
)

link_directories (
    ${EXTERNAL_LIB_DIR}
)

set(3rd_libraries
  efl
  objectmodel
  panning
  pml
  rbbl
  rcl
  rrl
  visr
)
set(available_libraries)
foreach(library IN LISTS 3rd_libraries)
  file( GLOB cmake_varibale ${EXTERNAL_LIB_DIR}/*${library}*)
  list(APPEND available_libraries ${cmake_varibale})
endforeach()

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED iamf2bear.cpp)
  target_link_libraries (${PROJECT_NAME} ${3rd_libraries} bear ear)
endif()

set(IAMF2RESONANCE_PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/iamf_bear_api.h)
set_target_properties(iamf2bear PROPERTIES PUBLIC_HEADER "${IAMF2RESONANCE_PUBLIC_HEADER}")

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/binaural
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/bear)

install(FILES ${available_libraries}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/binaural)

