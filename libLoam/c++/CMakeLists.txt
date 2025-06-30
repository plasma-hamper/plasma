# /* (c) 2010-2014 Oblong Industries */

project(Loam++)

#------------- source files -----------

set(
  Loam++_SOURCES

  AnkleObject.cpp
  ArgParse.cpp
  AxisAlignedBox.cpp
  FatherTime.cpp
  Frustum.cpp
  GeomSlab.cpp
  GeomTransform.cpp
  Line.cpp
  LoamxxRetorts.cpp
  Matrix44.cpp
  ObColor.cpp
  ObHormel.cpp
  ObInfo.cpp
  ObMap.cpp
  ObPseudopod.cpp
  ObResolvedPath.cpp
  ObRetort.cpp
  ObRetortPod.cpp
  ObTrove.cpp
  ObUniqueTrove.cpp
  Plane.cpp
  Quat.cpp
  Rectangle.cpp
  Sphere.cpp
  Str.cpp
  StrIterator.cpp
  StrMatchData.cpp
  StrPath.cpp
  Vect.cpp
  Vect4.cpp
  ob-coretypes-hash.cpp
  ob-math-utils.cpp
)
if (WIN32)
   LIST(APPEND Loam++_SOURCES win32/Mutex_win32.cpp)
else()
   LIST(APPEND Loam++_SOURCES Mutex.cpp)
endif()

set (
  Loam++_HEADERS

  AnkleObject.h
  ankle-object-interface-specialization.h
  ArgParse.h
  AxisAlignedBox.h
  CrawlIterator.h
  ErrnoSaver.h
  FatherTime.h
  Frustum.h
  GeomSlab.h
  GeomTransform.h
  Line.h
  LoamForward.h
  LoamStreams.h
  LoamxxRetorts.h
  Matrix44.h
  Mutex.h
  ObAcacia.h
  ObColor.h
  ObCons.h
  ob-coretypes-hash.h
  ObCrawl.h
  ob-enumclass.h
  ObHasher.h
  ObHormel.h
  ObInfo.h
  ObMap.h
  ob-math-utils.h
  ObPseudopod.h
  ObRef.h
  ObResolvedPath.h
  ObRetort.h
  ObRetortPod.h
  ObTrove.h
  ob-typeof.h
  ObUniqueTrove.h
  ObWeakRef.h
  OC_Functionary.h
  OC_Helpy.h
  OC_Undeffy.h
  OH_Functionary.h
  OM_Functionary.h
  OM_Helpy.h
  OT_Functionary.h
  OT_Helpy.h
  patella-macros.h
  Plane.h
  Preterite.h
  Quat.h
  Rectangle.h
  Sphere.h
  Str.h
  StrIterator.h
  StrPath.h
  Vect4.h
  Vect.h
)

#------------- dependencies -----------

#FIXME: we don't really need to link boost's regex lib here
set(Loam++_LINK_LIBS ${PLATFORM_LINK_LIBS} Loam ${ICU_LIBRARIES} ${Boost_LIBRARIES})

ObGeneratePC (
  NAME libLoam++
  VERSION ${PC_VERSION}
  DESCRIPTION "Part of Oblong's g-speak Spatial Operating Environment"
  # FIXME: use REQUIRES instead of LIBS and CFLAGS for Boost and ICU
  REQUIRES libLoam
  LIBS "-lLoam++ ${Boost_LIBRARIES_SPACE_SEP} ${ICU_LIBRARIES_SPACE_SEP}"
  CFLAGS "-I${Boost_INCLUDE_DIRS}"
)

#------------- library -----------

include_directories(${Loam++_SOURCE_DIR})

# Add ICU include directories
if(APPLE)
  set(ICU_ROOT /opt/homebrew/opt/icu4c)
  include_directories(${ICU_ROOT}/include)
endif()

add_library(Loam++ ${LIBRARY_TYPE} ${Loam++_SOURCES})
target_link_libraries(Loam++ ${Loam++_LINK_LIBS})
set_target_properties(Loam++ PROPERTIES
   COMPILE_DEFINITIONS "OB_LOAMXX_EXPORTS"
   SOVERSION 2 VERSION 2.0.0
   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

#------------- install -----------

install (
  FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/libLoam++.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install (
  FILES ${Loam++_HEADERS}
  DESTINATION "include/libLoam/c++"
)

install (
  TARGETS Loam++
  RUNTIME DESTINATION "bin"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if (MSVC)
  install(
    FILES ${Loam++_BINARY_DIR}/${BIN_SUBDIR}/Loam++.pdb
    DESTINATION "bin"
    CONFIGURATIONS Debug RelWithDebInfo
  )
endif()

#--------- subdirs -----------------
add_subdirectory(samples)
if (BUILD_TESTS)
    add_subdirectory(tests)
endif ()
