# /* (c) 2010 Oblong Industries */

cmake_minimum_required(VERSION 3.6.2)
project(Plasma)

# Avoid cmake warnings on Mac
if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif ()

#------------- source files -----------

set(Plasma_LINK_LIBS)

set(
  Plasma_SOURCES

  eintr-helper.c
  plasma-util.c
  pool.c
  pool_cmd.c
  pool-context.c
  pool-fifo.c
  pool-log.c
  pool_mmap.c
  pool-mmap-header.c
  pool_multi.c
  pool_net.c
  pool_options.c
  pool-portable.c
  pool_tcp.c
  pool-toc.c
  protein.c
  slaw.c
  slaw-coerce.c
  slaw-concat.c
  slaw-interop.c
  slaw-io.c
  slaw-io-convenience.c
  slaw-io-file.c
  slaw-numerics.c
  slaw-ordering.c
  slaw-path.c
  slaw-string.c
  slaw-v1.c
  slaw-walk.c
)
if (WIN32)
  list(APPEND Plasma_SOURCES
    win32/fifo_ops_win32.c
    win32/sem_ops_win32.c
  )
else()
  list(APPEND Plasma_SOURCES
    fifo_ops.c
    pool-flock-ops.c
    pool-lock-ops.c
    sem_ops.c
  )
endif()

if (OPENSSL_FOUND)
  message("found SSL and crypto, including...")
  list(APPEND Plasma_LINK_LIBS ${OPENSSL_LIBRARIES})
  list(
    APPEND Plasma_SOURCES
    ossl/ossl-client.c
    ossl/ossl-common.c
    ossl/ossl-eintr-helper.c
    ossl/ossl-io.c
    ossl/ossl-locking.c
    ossl/ossl-server.c
    ossl/ossl-validation.c
  )
  if (APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=deprecated")        # FIXME: including openssl deprecated on macosx 10.9
  endif()
else()
  list(APPEND Plasma_SOURCES nossl.c)
endif()

if ("${YAML_LIB}" STREQUAL YAML_LIB-NOTFOUND)
  message(WARNING "YAML not found, using stub")
  list(APPEND Plasma_SOURCES slaw-yaml-stub.c)
else()
  list(APPEND Plasma_SOURCES slaw-yaml.c)
  list(APPEND Plasma_LINK_LIBS ${YAML_LIB})
endif()

set (
  Plasma_HEADERS

  c-plasma.h
  plasma-retorts.h
  plasma-types.h
  pool_cmd.h
  pool.h
  pool-log.h
  pool_options.h
  pool-time.h
  protein.h
  slaw-coerce.h
  slaw.h
  slaw-interop.h
  slaw-io.h
  slaw-numeric-ilk-rumpus.h
  slaw-ordering.h
  slaw-path.h
  slaw-string.h
  slaw-walk.h
)

#------------- dependencies -----------

list(APPEND Plasma_LINK_LIBS Loam)

ObGeneratePC (
  NAME libPlasma
  VERSION ${PC_VERSION}
  DESCRIPTION "Data serialization and IPC library"
  REQUIRES libLoam
  LIBS "-lPlasma ${YAML_LIB} ${OPENSSL_LIBRARIES}"
)

#------------- library -----------

include_directories(${Loam_SOURCE_DIR})
include_directories(${Plasma_SOURCE_DIR})

# Add ICU include directories
if(APPLE)
  set(ICU_ROOT /opt/homebrew/opt/icu4c)
  include_directories(${ICU_ROOT}/include)
endif()

add_library(Plasma ${LIBRARY_TYPE} ${Plasma_SOURCES})
target_link_libraries(Plasma ${Plasma_LINK_LIBS})
set_target_properties(Plasma PROPERTIES
   COMPILE_DEFINITIONS "OB_PLASMA_EXPORTS"
   SOVERSION 2 VERSION 2.0.0
   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
)

#------------- programs -----------

set(
  Plasma_PROGRAMS

  pool_tcp_server
  p-await
  p-deposit
  p-nth
  p-create
  p-newest-idx
  p-oldest-idx
  p-rename
  p-sleep
  p-stop
  p-list
  p-info
  p-resize
)
if (NOT YAML_LIB_NOTFOUND)
LIST(APPEND
  Plasma_PROGRAMS
  bin2yaml
  yaml2bin
)
endif()

if (WIN32)
  LIST(APPEND
    Plasma_PROGRAMS
    fire-and-forget
  )
endif()

foreach (prog ${Plasma_PROGRAMS})
  add_executable(${prog} ${prog}.c)
  target_link_libraries(${prog} Plasma Loam)
endforeach(prog)

# matrix_test is called by scripts in both the "tests" and "perf" directories,
# so keep it here in the parent directory.  Similarly, await_test is used
# from both the "tests" and "tests-mmap-only" directories, and ob-lsfd is
# used from both "tests" and "t".  fill_pool is only used by the
# scripts in the "perf" directory, but since currently that directory has
# no Makefile and no executables (just scripts), I'll keep fill_pool here
# for now.  The other two executables here (multi_test and simple_read)
# are not installed and are not used by any tests, so their usefulness is
# questionable... they are probably best regarded as "sample programs".

set(
  Plasma_noinst_PROGRAMS

  await_test
  fill_pool
  matrix_test
  multi_test
)
if (NOT WIN32)
  list(APPEND Plasma_noinst_PROGRAMS
    ob-lsfd
    simple_read
  )
endif()

foreach (prog ${Plasma_noinst_PROGRAMS})
  add_executable(${prog} ${prog}.c)
  target_link_libraries(${prog} Plasma Loam)
endforeach(prog)

#------------- install -----------

pod2manhtml(p-list.pod 1)

install (
  FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/libPlasma.pc
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

install (
  FILES ${Plasma_HEADERS}
  DESTINATION "include/libPlasma/c"
)

install (
  FILES ob-plasma-cert.sh
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install (
  TARGETS Plasma ${Plasma_PROGRAMS}
  RUNTIME DESTINATION "bin"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if (MSVC)
  install(
    FILES ${Plasma_BINARY_DIR}/${BIN_SUBDIR}/Plasma.pdb
    DESTINATION "bin"
    CONFIGURATIONS Debug RelWithDebInfo
  )
endif()

#--------- subdirs -----------------

if (BUILD_TESTS)
  # "tests" is for tests that apply to both mmap pools and network pools
  # "tests-mmap-only" is for tests the only apply to mmap pools but not network
  # "t" is for tests that don't use pools at all (i. e. slaw tests)
  add_subdirectory(${Plasma_SOURCE_DIR}/t)
  add_subdirectory(${Plasma_SOURCE_DIR}/tests)
  # Skip some tests that aren't happy on windows yet
  if (NOT WIN32)
    add_subdirectory(${Plasma_SOURCE_DIR}/tests-mmap-only)
  endif()
endif()
