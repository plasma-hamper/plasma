# /* (c) 2014 Oblong Industries */

cmake_minimum_required(VERSION 2.8.9)
project(PlasmaZeroconf)

# Avoid cmake warnings on Mac
if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()

#------------- source files -----------

set(
  PlasmaZeroconf_SOURCES

  zeroconf-utils.c
  PoolServer.cpp
  Zeroconf.cpp
)
if (APPLE)
  list(APPEND PlasmaZeroconf_SOURCES
    zeroconf-client-bonjour.c
    zeroconf-server-bonjour.c
  )
elseif(NOT WIN32)
  list(APPEND PlasmaZeroconf_SOURCES
    zeroconf-client-avahi.c
    zeroconf-server-avahi.c
  )
endif()

set (
  PlasmaZeroconf_HEADERS

  PoolServer.h
  Zeroconf.h
)

#------------- dependencies -----------

set(ZEROCONFDEPS "")
set(PlasmaZeroconf_LINK_LIBS Loam Plasma Loam++ Plasma++)
if (APPLE)
  # ?
elseif(WIN32)
  # ?
else()
  set(ZEROCONFDEPS "avahi-client")
  list(APPEND PlasmaZeroconf_LINK_LIBS "-lavahi-common -lavahi-client")
endif()

ObGeneratePC (
  NAME libPlasmaZeroconf
  VERSION ${PC_VERSION}
  DESCRIPTION "Zeroconf support for libPlasma"
  REQUIRES libLoam ${ZEROCONFDEPS}
  LIBS "-lPlasmaZeroconf ${YAML_LIB} ${OPENSSL_LIBRARIES}"
)

#------------- library -----------

add_library(PlasmaZeroconf ${LIBRARY_TYPE} ${PlasmaZeroconf_SOURCES})
target_link_libraries(PlasmaZeroconf ${PlasmaZeroconf_LINK_LIBS})
set_target_properties(PlasmaZeroconf PROPERTIES
   COMPILE_DEFINITIONS "OB_PLASMA_EXPORTS"
   SOVERSION 2 VERSION 2.0.0
   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

#------------- programs -----------

set(
  PlasmaZeroconf_PROGRAMS

  pool-server-zeroconf-adapter
)

foreach (prog ${PlasmaZeroconf_PROGRAMS})
  add_executable(${prog} ${prog}.cpp)
  target_link_libraries(${prog} PlasmaZeroconf Loam)
endforeach(prog)

#------------- install -----------

install (
  FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/libPlasmaZeroconf.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install (
  FILES ${PlasmaZeroconf_HEADERS}
  DESTINATION "include/libPlasma/zeroconf"
)

install (
  TARGETS PlasmaZeroconf ${PlasmaZeroconf_PROGRAMS}
  RUNTIME DESTINATION "bin"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if (MSVC)
  install(
    FILES ${PlasmaZeroconf_BINARY_DIR}/${BIN_SUBDIR}/PlasmaZeroconf.pdb
    DESTINATION "bin"
    CONFIGURATIONS Debug RelWithDebInfo
  )
endif()

#--------- subdirs -----------------

add_subdirectory(${PlasmaZeroconf_SOURCE_DIR}/tests)
