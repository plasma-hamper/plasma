# /* (c) 2010-2014 Oblong Industries */

project(Plasma++)

#------------- source files -----------

set(
  Plasma++_SOURCES

  AtomicComposite.cpp
  CompositeSlaw.cpp
  Hose.cpp
  HoseGang.cpp
  Pool.cpp
  Protein.cpp
  ProteinComposite.cpp
  Slaw.cpp
  SlawCons.cpp
  SlawIterator.cpp
  SlawList.cpp
  SlawMap.cpp
  SlawRef.cpp
)

set (
  Plasma++_HEADERS

  CompositeSlaw.h
  HoseGang.h
  Hose.h
  PlasmaForward.h
  Plasma.h
  PlasmaStreams.h
  PlasmaxxRetorts.h
  Pool.h
  Protein.h
  RefCounted.h
  Slaw.h
  SlawIterator.h
  SlawRef.h
  slaw-traits.h
)

#------------- dependencies -----------

set(Plasma++_LINK_LIBS ${PLATFORM_LINK_LIBS} Loam Plasma Loam++ ${ICU_LIBRARIES})

ObGeneratePC (
  NAME libPlasma++
  VERSION ${PC_VERSION}
  DESCRIPTION "Data serialization and IPC library (C++ API)"
  REQUIRES libLoam libLoam++ libPlasma
  LIBS -lPlasma++
)

#------------- library -----------

include_directories(${Plasma++_SOURCE_DIR})

add_library(Plasma++ ${LIBRARY_TYPE} ${Plasma++_SOURCES})
target_link_libraries(Plasma++ ${Plasma++_LINK_LIBS})
set_target_properties(Plasma++ PROPERTIES
   COMPILE_DEFINITIONS "OB_PLASMAXX_EXPORTS"
   SOVERSION 2 VERSION 2.0.0
   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

#------------- install -----------

install (
  FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/libPlasma++.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install (
  FILES ${Plasma++_HEADERS}
  DESTINATION "include/libPlasma/c++"
)

install (
  FILES ${Staging_Plasma++_HEADERS}
  DESTINATION "include/libStaging/libPlasma/c++"
)

install (
  TARGETS Plasma++
  RUNTIME DESTINATION "bin"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if (MSVC)
  install(
    FILES ${Plasma++_BINARY_DIR}/${BIN_SUBDIR}/Plasma++.pdb
    DESTINATION "bin"
    CONFIGURATIONS Debug RelWithDebInfo
  )
endif()

#--------- subdirs -----------------

add_subdirectory(samples)
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()
