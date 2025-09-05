# /* (c) 2010-2014 Oblong Industries */

project(Loam)

#------------- source files -----------

set(
  Loam_SOURCES

  ob-atomic.c
  ob-dirs.c
  ob-file.c
  ob-log.c
  ob-math.c
  ob-rand.c
  ob-retorts.c
  ob-string.c
  ob-thread.c
  ob-time.c
  ob-truly-rand.c
  ob-util.c
  ob-vers.c
  datadir.c
  prefix.c
)
if (WIN32)
  list(APPEND Loam_SOURCES win32/ob-sys-win32.c)
  list(APPEND Loam_SOURCES win32/ob-sys-win32-cpp.cpp)
endif()
if (UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  list(APPEND Loam_SOURCES ob-hash-64.S)
  set_property(SOURCE ob-hash-64.S PROPERTY LANGUAGE C)
elseif (UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_32")
  list(APPEND Loam_SOURCES ob-hash-32.S)
  set_property(SOURCE ob-hash-32.S PROPERTY LANGUAGE C)
else ()
  list(APPEND Loam_SOURCES ob-hash.c ob-hash-city.c)
endif ()

set (
  Loam_HEADERS

  ob-api.h
  ob-atomic.h
  ob-attrs.h
  ob-coretypes.h
  ob-dirs.h
  ob-endian.h
  ob-file.h
  ob-geom.h
  ob-hash.h
  ob-log.h
  ob-math.h
  ob-mvtypes.h
  ob-pthread.h
  ob-rand.h
  ob-retorts.h
  ob-string.h
  ob-sys.h
  ob-thread.h
  ob-time.h
  ob-types.h
  ob-util.h
  ob-vers-gen.h
  ob-vers.h
)

configure_file(
  "${Loam_SOURCE_DIR}/ob-vers-gen.h.in"
  "${Loam_SOURCE_DIR}/ob-vers-gen.h"
)

set(Loam_PROGRAMS
  ob-version
  ob-which
)

#------------- dependencies -----------

set(Loam_LINK_LIBS ${PLATFORM_LINK_LIBS})

if (WIN32)
    set(PTHREAD_OPT "")
else()
    set(PTHREAD_OPT "-pthread")
endif()

# Include SANITIZER_FLAGS because otherwise
# compile_extras step explodes running rubysmoke sample.
#
# Include old non-multilib libdir to be kind to downstream
# packages (like libWebThing) which install their libraries
# to the old libdir, and never added their own rpath.
#
# Removed CMAKE_INSTALL_FULL_LIBDIR from LIBS, because it was
# the empty string, and having the flags "-L -Wl,-rpath," was
# causing all sorts of trouble.  Not sure what is supposed to
# set CMAKE_INSTALL_FULL_LIBDIR, but it wasn't getting set.
ObGeneratePC (
  NAME libLoam
  VERSION ${PC_VERSION}
  DESCRIPTION "General utility functions"
  LIBS "-L${CMAKE_INSTALL_PREFIX}/lib -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib -lLoam ${PLATFORM_LINK_LIBS} ${SANITIZER_FLAGS}"
  CFLAGS "-I${YOBUILD}/include -I${CMAKE_INSTALL_PREFIX}/include ${PTHREAD_OPT} ${SANITIZER_FLAGS}"
)

#------------- cflags -----------

if (NOT WIN32)
  ObCheckCFlags(CMAKE_C_FLAGS
    -Wno-write-strings           # FIXME: ob-log.c fails to build with -Wwrite-strings
  )

  set(dSFMT_SPECIAL_CFLAGS )
  set(FLAGS
     -fno-strict-aliasing
     -std=gnu99
     -msse2
     -fPIC
  )
  if (NOT APPLE)
      # Work around http://public.kitware.com/Bug/view.php?id=15526
      LIST(APPEND FLAGS "-finline-functions" "--param max-inline-insns-single=1800")
  endif()
  ObCheckCFlags(dSFMT_SPECIAL_CFLAGS ${FLAGS})
endif()

#------------- library -----------

include_directories(${Loam_SOURCE_DIR})

# Use a convenience library here so we can set special compile options for one file
# FIXME: can't we just do
# set_source_files_properties(dSFMT.c PROPERTIES COMPILE_FLAGS "${dSFMT_SPECIAL_CFLAGS}") ?
add_library(dSFMT OBJECT dSFMT.c)
if (NOT WIN32)
   set_target_properties(
     dSFMT
     PROPERTIES COMPILE_FLAGS
     "${dSFMT_SPECIAL_CFLAGS}"
   )
endif()
set_target_properties(dSFMT PROPERTIES COMPILE_DEFINITIONS "OB_LOAM_EXPORTS")

add_library(Loam ${LIBRARY_TYPE} $<TARGET_OBJECTS:dSFMT> ${Loam_SOURCES})
target_link_libraries(Loam ${Loam_LINK_LIBS})
if (WIN32)
    target_link_libraries(Loam ws2_32.lib)
endif()
set_target_properties(Loam PROPERTIES
   COMPILE_DEFINITIONS "OB_LOAM_EXPORTS"
   SOVERSION 2 VERSION 2.0.0
   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

#------------- programs -----------

foreach (prog ${Loam_PROGRAMS})
  add_executable(${prog} ${prog}.c)
  target_link_libraries(${prog} Loam)
endforeach(prog)

#------------- install -----------

pod2manhtml(ob-version.pod 1)

install (
  FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/libLoam.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install (
  FILES ${Loam_HEADERS}
  DESTINATION "include/libLoam/c"
)

if (WIN32)
  install (
    FILES win32/ob-pthread-win32.h win32/ob-sys-win32.h
    DESTINATION "include/libLoam/c/win32"
  )
endif()

install (
  TARGETS Loam ${Loam_PROGRAMS}
  RUNTIME DESTINATION "bin"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if (MSVC)
  install(
    FILES ${Loam_BINARY_DIR}/${BIN_SUBDIR}/Loam.pdb
    DESTINATION "bin"
    CONFIGURATIONS Debug RelWithDebInfo
  )
endif()

#--------- subdirs -----------------
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()
