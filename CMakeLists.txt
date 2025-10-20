cmake_minimum_required (VERSION 3.6.2)

project(g-speak CXX C)

# Compatibility options
option(PLASMA_LEGACY_MODE "Build for compatibility with older systems" OFF)
option(PLASMA_ENABLE_TCP_OPTIMIZATIONS "Enable TCP performance optimizations" ON)
option(PLASMA_ENABLE_LARGE_TRANSFER_LOGGING "Enable logging for transfers >1MB" ON)

# We don't really have a YOBUILD anymore, but it causes some
# annoyance if YOBUILD is the empty string, so let's just set
# it to something...
set(YOBUILD "/opt/yobuild")

# Rather than just hardcoding "plasma", have a variable for it:
set(PRODNAME "plasma")

# Auto-detect if legacy mode needed
if(NOT DEFINED PLASMA_LEGACY_MODE)
    # Check compiler version
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
        set(PLASMA_LEGACY_MODE ON)
        message(STATUS "GCC < 5.0 detected - enabling legacy compatibility mode")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.4")
        set(PLASMA_LEGACY_MODE ON)
        message(STATUS "Clang < 3.4 detected - enabling legacy compatibility mode")
    elseif(DEFINED ENV{PLASMA_LEGACY_BUILD})
        set(PLASMA_LEGACY_MODE ON)
        message(STATUS "PLASMA_LEGACY_BUILD environment variable set - enabling legacy compatibility mode")
    endif()
endif()

# Select C++ dialect based on mode
if(PLASMA_LEGACY_MODE)
    set(CMAKE_CXX_STANDARD 11)
    message(STATUS "Building in legacy compatibility mode (C++11)")
else()
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "Building with modern features (C++17)")
endif()
set(CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get access to g-speak's standard cmake options, helper functions,
# and variables.
# Clients use FIND_PACKAGE(Oblong), but we grab it here from source tree.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/bld/cmake")
INCLUDE("ObBoilerplate")
# ObGenerateProject not included in ObBoilerplate as it's a bit specialized.
INCLUDE("ObGenerateProject")
# Short name
SET(TOP_TEMPLATE ${CMAKE_SOURCE_DIR}/samples/obi-template)


set(top_srcdir "${g-speak_SOURCE_DIR}")
set(abs_top_srcdir "${g-speak_SOURCE_DIR}")
set(top_builddir "${g-speak_BINARY_DIR}")
set(abs_top_builddir "${g-speak_BINARY_DIR}")

# Tell tests where top of source tree is.
# Consumed by libStaging/include/private/testHelpers.h
# and libLoam/c/tests/test-paths.c
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DABS_TOP_SRCDIR=\\\"${abs_top_srcdir}\\\"")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DABS_TOP_SRCDIR=\\\"${abs_top_srcdir}\\\"")


# Set variables needed by libLoam/c/CMakeLists.txt and libLoam/c/ob-vers-gen.h.in
# exec_program(
#     ${SHELL}
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ARGS "bld/cmake/get-version.sh version"
#     OUTPUT_VARIABLE VERSION
# )
# exec_program(
#     ${SHELL}
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ARGS "bld/cmake/get-version.sh changenum"
#     OUTPUT_VARIABLE PACKAGE_PATCH
# )
# set(PACKAGE_VERSION_PATCH "${VERSION}-${PACKAGE_PATCH}")

# For now, just hardcode the version here:
set (VERSION "5.6.1")
set (PACKAGE_VERSION_PATCH "${VERSION}")

# And I don't know what was supposed to set PC_VERSION, but it looks
# like nothing was setting it, so let's set it here:
set (PC_VERSION "${VERSION}")

# Derive some additional variables from VERSION
STRING(REPLACE "." ";" VERSION_LIST ${VERSION})
LIST(GET VERSION_LIST 0 MAJOR_VERSION)
LIST(GET VERSION_LIST 1 MINOR_VERSION)
LIST(LENGTH VERSION_LIST VERSION_LIST_LEN)
if (VERSION_LIST_LEN LESS 3)
  set(MICRO_VERSION 0)
else()
  LIST(GET VERSION_LIST 2 MICRO_VERSION)
endif()
execute_process(
  COMMAND "perl" "-e" "printf('%u%02u%02u', ${MAJOR_VERSION}, ${MINOR_VERSION}, ${MICRO_VERSION})"
  OUTPUT_VARIABLE NUMBER_VERSION
)

if(WIN32)
    set(YAML_LIB "-libpath:${YOBUILD}/lib;yaml.lib")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DYAML_DECLARE_STATIC" )  # yobuild12
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DYAML_DECLARE_STATIC" )  # yobuild12
    set(ICU_LIBRARIES "-libpath:${YOBUILD}/lib;icuin.lib;icuuc.lib" )
else()
    # Using absolute paths to libraries causes great woe in .pc files
    # (in particular, pkg-config reorders absolute paths before -l libraries,
    #  causing link failures), so don't do things the cmake way here:
    #find_library(YAML_LIB yaml HINTS "${YOBUILD}/lib")
    #find_package(ICU REQUIRED COMPONENTS uc i18n)
    # Instead, use -l flags.   We already have all needed -L and -I flags.
    set(YAML_LIB "-lyaml")
    set(ICU_LIBRARIES "-licuuc -licui18n")
endif()
# Replace semicolons with spaces to keep cmake from splitting the argument.
STRING(REPLACE ";" " " ICU_LIBRARIES_SPACE_SEP "${ICU_LIBRARIES}" )

# otherwise boost includes optimized and debug versions, which fouls up the build
set(Boost_USE_DEBUG_RUNTIME FALSE)
set(BOOST_ROOT ${YOBUILD})
find_package(Boost REQUIRED COMPONENTS filesystem regex)
if (NOT WIN32)
    # Using absolute paths to libraries causes great woe in .pc files
    # Instead, use -l flags.   We already have all needed -L and -I flags.
    STRING(REGEX REPLACE "/[a-zA-Z0-9_/-]*/libboost" "-lboost" Boost_LIBRARIES "${Boost_LIBRARIES}" )
    STRING(REGEX REPLACE ".so|.dylib" "" Boost_LIBRARIES "${Boost_LIBRARIES}" )
endif()
STRING(REPLACE ";" " " Boost_LIBRARIES_SPACE_SEP "${Boost_LIBRARIES}" )
message("Boost_LIBRARIES is ${Boost_LIBRARIES}")
message("Boost_INCLUDE_DIRS is ${Boost_INCLUDE_DIRS}")
# Everything needs Boost...
include_directories(${Boost_INCLUDE_DIRS})
# Boost doesn't know where it lives, so turn off autolinking...
add_definitions("-DBOOST_ALL_NO_LIB")
# Avoid deprecated unused variables 'boost::system::posix_category'
add_definitions("-DBOOST_SYSTEM_NO_DEPRECATED")

INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckCXXSourceCompiles)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckCXXCompilerFlag)

# Feature detection for compatibility
if(PLASMA_LEGACY_MODE)
    set(PLASMA_LEGACY_BUILD 1)
endif()

# Check for C++ features
check_cxx_source_compiles("
    #include <random>
    #include <algorithm>
    #include <vector>
    int main () {
        // std::vector<int> v = {1,2,3,4,5};  // cain't when compiling < c++-11
        std::vector <int> v;
        v.push_back (1);  v.push_back (2);
        v.push_back (3);  v.push_back (4);
        v.push_back (5);
        std::mt19937 gen;
        std::shuffle (v.begin (), v.end (), gen);
        return 0;
    }" HAVE_STD_SHUFFLE)

check_cxx_source_compiles("
    #include <random>
    int main() {
        std::mt19937 gen;
        return 0;
    }" HAVE_STD_MT19937)

# Check for TCP features
if(NOT WIN32)
    check_symbol_exists(TCP_USER_TIMEOUT "netinet/tcp.h" HAVE_TCP_USER_TIMEOUT)
    check_symbol_exists(TCP_KEEPINTVL "netinet/tcp.h" HAVE_TCP_KEEPINTVL)
    check_symbol_exists(TCP_KEEPIDLE "netinet/tcp.h" HAVE_TCP_KEEPIDLE)
endif()

# Configure plasma_config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plasma_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/plasma_config.h)
if (UNIX)
  set(OPENSSL_LIBRARIES "-lssl -lcrypto")
  set(OPENSSL_FOUND TRUE)

  check_include_file(openssl/ec.h HAVE_OPENSSL_EC_H)
  check_include_file(openssl/ecdsa.h HAVE_OPENSSL_ECDSA_H)

  if (HAVE_OPENSSL_EC_H AND HAVE_OPENSSL_ECDSA_H)
    set(HAVE_OPENSSL_ECC 1)
  endif()
endif()
if (WIN32)
  set(OPENSSL_LIBRARIES "ssleay32.lib;libeay32.lib" )
  set(OPENSSL_FOUND TRUE)
endif()
if (OPENSSL_FOUND)
  add_definitions("-DHAVE_OPENSSL")
endif()

set(PLATFORM_LINK_LIBS)
if (UNIX)
  if (NOT APPLE)
    find_library(RT_LIB rt)
    list(APPEND PLATFORM_LINK_LIBS ${RT_LIB})
    find_library(DL_LIB dl)
    list(APPEND PLATFORM_LINK_LIBS ${DL_LIB})
    find_library(MATH_LIB m)
    list(APPEND PLATFORM_LINK_LIBS ${MATH_LIB})
    list(APPEND PLATFORM_LINK_LIBS pthread)
  endif()
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR};${YOBUILD}/lib")

  # On Linux, we also need to add YOBUILD/lib to rpath *before* install
  # (else e.g. FontTest fails to load becausel ibMedia can't find libgnutls).
  # On Mac, 'make check' fails with
  #  system/libProtist/tests/ProtistPatellaRegistryTest
  #  dyld: Library not loaded: @rpath/libboost_filesystem-mt.dylib
  # on some macs (not sure why it isn't everywhere).
  # So:
  list(APPEND PLATFORM_LINK_LIBS -L${YOBUILD}/lib -Wl,-rpath,${YOBUILD}/lib)

endif()

#------------- cflags ---------------

set(VISIBILITY_FLAGS )

if (WIN32)
    #set(ERROR_CFLAGS "/WX")
    #set(ERROR_CXXFLAGS "/WX")
    # /wdXXXX disables a particular warning for visual C++
    set(FLAGS
       /wd4018   # signed/unsigned
       /wd4244   # FIXME: Ignore precision warnings to allow compilation on 32 bit systems?!
       /wd4251   # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
       /wd4305   # truncation from double to float
       /wd4345   # double initialization warning
       /wd4800   # truncation to bool
       /wd4996   # Ignore warnings about POSIX function names being deprecated.  Last I heard, POSIX didn't deprecate them.
       # Warnings which need attention before we can turn on /WX
       #/wd4047
       #/wd4090
       #/wd4133
       #/wd4267
       #/wd4273
       #/wd4275
       #/wd4311
       #/wd4312
       #/wd4334
       #/wd4838
    )
else()
    set(ERROR_CFLAGS -Werror )
    set(ERROR_CXXFLAGS -Werror )
    set(FLAGS
      -fdiagnostics-show-option
      -Wall
      -Wempty-body
      -Wpointer-arith
      -Wshadow
      -Wstrict-prototypes
      -Wsuggest-override #clang will use inconsistent-missing-override, by default
      -Wtype-limits
      -Wvla
      -Wwrite-strings
      # Following few are just too strict for now, disable unconditionally
      -Wno-error=comment           # libOuija
      -Wno-error=shadow            # FIXME: libNoodoo, issue 242
      -Wno-error=deprecated-declarations
      -Wno-error=format-truncation # FIXME: pool_fifo_wake_awaiters
      -Wno-error=sign-compare      # FIXME: plasma-gstreamer.c poolmediasrc.c
      -Wno-strict-overflow
      )

    # See bug 10960
    ObCheckCXXFlags(VISIBILITY_FLAGS
      -fvisibility-inlines-hidden
      -fvisibility=hidden
    )
endif()

# Turn the wish list FLAGS into the supported subset ERROR_CFLAGS and ERROR_CXXFLAGS.
ObCheckCFlags(ERROR_CFLAGS ${FLAGS})
ObCheckCXXFlags(ERROR_CXXFLAGS ${FLAGS})

set(OPTIMIZE_FLAGS )
if (WIN32)
    # Avoid breaking Boost, at the cost of turning off error checking?
    if (CMAKE_CXX_FLAGS_DEBUG MATCHES "/RTC1")
        message(STATUS "Removing /RTC1 from CMAKE_CXX_FLAGS_DEBUG")
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    endif()
    if (CMAKE_C_FLAGS_DEBUG MATCHES "/RTC1")
        string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
        message(STATUS "Removing /RTC1 from CMAKE_C_FLAGS_DEBUG")
    endif()
    # FIXME: avoid those nasty security warnings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
    # Our C++ .h files use M_PI
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_USE_MATH_DEFINES" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /showIncludes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /showIncludes")

    # binary location differs with generator
    if (WIN32 AND NOT CMAKE_GENERATOR MATCHES Ninja)
        set(BIN_SUBDIR ${CMAKE_BUILD_TYPE})
    else()
        set(BIN_SUBDIR ".")
    endif()
else()
    if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
       set(OPTIMIZE_FLAGS "-g0 -O3 -DNDEBUG")
    elseif (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
       if (TSAN OR ASAN)
           # Sanitizers have overhead, compensate with a little optimization
           set(OPTIMIZE_FLAGS "-g -O1 -UNDEBUG")
       else()
           set(OPTIMIZE_FLAGS "-g -O0 -UNDEBUG")
       endif()
    elseif (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
       set(OPTIMIZE_FLAGS "-g -O3 -UNDEBUG")
    else()
       message(FATAL_ERROR "Unknown build type ${CMAKE_BUILD_TYPE}")
    endif()
    if(FORCE_PIC)
       message("Forcing -fPIC")
       set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    endif()
    if (NOT APPLE)
      if (${CMAKE_DEB_BUILD_ARCH} MATCHES "armhf")
          message("Telling compiler to use less RAM; our Pi buildbot is small")
          # (bs_detect_ncores also caps 'make -j' to one per ~130MB of RAM)
          # Use less ram on diagnostics, and collect garbage when it goes
          # over 64MB
          set(LOWRAM_FLAGS "-ftrack-macro-expansion=0 --param ggc-min-expand=30 --param ggc-min-heapsize=65536")
          set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LOWRAM_FLAGS}")
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOWRAM_FLAGS}")
      endif()
    endif()
endif()

# Pass CMAKE_INSTALL_PREFIX through to prefix.c
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DQUOTED_PREFIX=\\\"${CMAKE_INSTALL_PREFIX}\\\"")

# Let source files use top-relative include path
include_directories(${g-speak_BINARY_DIR})
# Include directory for generated plasma_config.h
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${g-speak_SOURCE_DIR})
include_directories(${g-speak_SOURCE_DIR}/system)
include_directories(SYSTEM ${g-speak_SOURCE_DIR}/gtest/include)

#------------- generate documentation -----------

# Usage: win2unix name-of-output-variable value-of-path-to-convert-from-native
MACRO(native2unix _outdir _indir)
  set(${_outdir} ${_indir})
  if (WIN32)
    # For cygwin, turn C: into /cygdrive/c, and \ into /
    # FIXME: don't only work for drives C and E
    STRING(REGEX REPLACE "[Cc]:" "/cygdrive/c" ${_outdir} "${${_outdir}}")
    STRING(REGEX REPLACE "[Ee]:" "/cygdrive/e" ${_outdir} "${${_outdir}}")
    STRING(REPLACE "\\" "/" ${_outdir} "${${_outdir}}")
  endif()
  #message("native2unix input: ${_indir}, output ${${_outdir}}")
ENDMACRO()

# Given a .pod file, create .1 and .html files in share/doc
# and update share/doc/index.html
set(doc_bindir "${g-speak_BINARY_DIR}/share/doc")
FILE(MAKE_DIRECTORY "${doc_bindir}/htmlman")
native2unix(_doc_bindir "${doc_bindir}")

MACRO(pod2manhtml _sourcefile _manext)
    GET_FILENAME_COMPONENT(_basefile ${_sourcefile} NAME_WE)
    set(_outman ${_basefile}.${_manext})
    set(_outhtml ${_basefile}-man.html)
    native2unix(_abssourcefile "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcefile}")
    if (WIN32)
      # We use native cmake and ninja, not cygwin versions.
      # Work around lack of shebang support in windows for invoking cygwin pgms
      # by assuming cygwin perl + pod2man installed
      set(pod2mancmd "C:/cygwin64/bin/perl;/usr/bin/pod2man")
      set(pod2htmlcmd "C:/cygwin64/bin/perl;/usr/bin/pod2html")
      set(indexcmd "C:/cygwin64/bin/perl;${g-speak_SOURCE_DIR}/bld/make-html-doc-index.pl")
    else()
      set(pod2mancmd "pod2man")
      set(pod2htmlcmd "pod2html")
      set(indexcmd "${g-speak_SOURCE_DIR}/bld/make-html-doc-index.pl")
    endif()
    add_custom_target(${_outman}-target ALL
        DEPENDS ${doc_bindir}/${_outman}
    )
    add_custom_command(
        OUTPUT ${doc_bindir}/${_outman}
        DEPENDS ${_sourcefile}
        COMMAND ${pod2mancmd} "--section=${_manext}" "--center=User Commands" "--release=${PRODNAME} ${VERSION}" "${_abssourcefile}" "${_doc_bindir}/${_outman}"
        COMMAND ${pod2htmlcmd} "--infile=${_abssourcefile}" "--outfile=${_doc_bindir}/htmlman/${_outhtml}"
        COMMAND ${indexcmd} -o ${_doc_bindir}/index.html ${_doc_bindir}/htmlman/*-man.html
        VERBATIM
    )
    INSTALL(FILES ${doc_bindir}/htmlman/${_outhtml} DESTINATION share/doc/${PRODNAME}/htmlman )
    INSTALL(FILES ${doc_bindir}/${_outman}          DESTINATION share/man/man${_manext} )
ENDMACRO(pod2manhtml)
INSTALL(FILES ${doc_bindir}/index.html DESTINATION share/doc/${PRODNAME} )

#------------- test support ---------------

# List of all directories we need to add to the PATH to run tests
# (***On Windows, you must add all directories containing dlls, too***)
# FIXME: Is there a way to generate this (like, say, with find)?
# FIXME: this is one of two places we list all test directories.
set(
  ALL_BIN_DIRS

  gtest
  libBasement libBasement/tests
  libLoam/c libLoam/c/tests
  libLoam/c++ libLoam/c++/tests
  libPlasma/c++ libPlasma/c++/tests
  libPlasma/c libPlasma/c/tests libPlasma/c/t libPlasma/c/tests-mmap-only
  libPlasma/ruby
  libProtist libProtist/tests
)
if (WIN32 AND NOT CMAKE_GENERATOR MATCHES Ninja)
  foreach(dir ${ALL_BIN_DIRS})
    LIST(APPEND ALL_BIN_DIRS_ABS ${g-speak_BINARY_DIR}/${dir}/${CMAKE_BUILD_TYPE})
    LIST(APPEND ALL_BIN_DIRS_ABS ${g-speak_SOURCE_DIR}/${dir})
  endforeach()
else()
  foreach(dir ${ALL_BIN_DIRS})
    LIST(APPEND ALL_BIN_DIRS_ABS ${g-speak_BINARY_DIR}/${dir})
    LIST(APPEND ALL_BIN_DIRS_ABS ${g-speak_SOURCE_DIR}/${dir})
  endforeach()
endif()
native2unix(ALL_BIN_DIRS_ABS_COLONSEP "${ALL_BIN_DIRS_ABS}")
# For Linux/Mac PATH, join the list with colons (i.e. turn cmake's list-member semicolons into colons)
STRING(REPLACE ";" ":" ALL_BIN_DIRS_ABS_COLONSEP "${ALL_BIN_DIRS_ABS_COLONSEP}")

# For Windows PATH, join the list with escaped semicolons
# WTF? STRING(REPLACE ";" "\\;" ALL_BIN_DIRS_ABS_SEMICOLONSEP "${ALL_BIN_DIRS_ABS}")
SET(ALL_BIN_DIRS_ABS_SEMICOLONSEP "${ALL_BIN_DIRS_ABS}")

set(CUSTOMFILE "CTestCustom.cmake.in")
# Arrange for global setup/teardown before/after all tests.
# Place CTestCustom.cmake anywhere developers might run ctest and need a pool server.
# FIXME: this is one of two places we list all test directories.
configure_file(${CUSTOMFILE} CTestCustom.cmake @ONLY)
configure_file(${CUSTOMFILE} libPlasma/c/CTestCustom.cmake @ONLY)
configure_file(${CUSTOMFILE} libPlasma/c++/CTestCustom.cmake @ONLY)
configure_file(${CUSTOMFILE} libPlasma/c/tests/CTestCustom.cmake @ONLY)
configure_file(${CUSTOMFILE} libPlasma/c++/tests/CTestCustom.cmake @ONLY)
configure_file(${CUSTOMFILE} libProtist/tests/CTestCustom.cmake @ONLY)

# Tell test wrapper where to find executables
configure_file(bld/cmake/yotest.in yotest @ONLY NEWLINE_STYLE LF)

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)

# Function to add a test and run it via yotest with the given fixtures.
# Usage: add_wrapped_test testname exe-to-run "${_myFixtures}"
# _myFixtures is a mandatory semicolon-separated list of test fixtures this test must pass against
# WARNING: you have to quote the third arg, or only the first element of the list will be passed!
function(add_wrapped_test _myTest _myTestRunner _myFixtures)
  add_test(
    NAME ${_myTest}
    COMMAND "${SHELL}" ${g-speak_BINARY_DIR}/yotest run "${_myFixtures}" ${_myTestRunner}
  )
endfunction()

# Function to generate tests.  All three arguments are quoted lists:
#  my_TESTS     - source files (without suffices), one per test
#  my_LINK_LIBS - libraries the tests need to link against
#  my_FIXTURES  - if present, list of test fixtures the tests must pass
#                 against (choices: local, tcp, tcpo, tcps).  If absent, uses 'local'.
# The macro searches for the source with C, C++, ruby, shell, and python suffixes.
# If there is both a c/C+++ source and a ruby/shell/python source file, it
# builds the c/c++ one and runs the interpreted one.
# In addition, if the test is a compiled c or C++ test, it will add any
# sources in ${test}_extra_SOURCES to the target.
# Spaces are not allowed in test names.
function(generate_tests my_TESTS my_LINK_LIBS)
 if (NOT BUILD_TESTS)
  MESSAGE("BUILD_TESTS is false, not generating tests for ${PROJECT_NAME}")
 else ()
  # Default to local fixture
  if (ARGN STREQUAL "")
     set(my_FIXTURES "local")
  else()
     set(my_FIXTURES "${ARGN}")
  endif()
  # Refer to arguments without quotes if you want to treat them as lists.
  foreach( test ${my_TESTS} )
    foreach( SUFFIX c cpp )
      if(EXISTS "${PROJECT_SOURCE_DIR}/${test}.${SUFFIX}")
        # Compiled test (possibly run via a helper script)
        add_executable(${test} ${test}.${SUFFIX} ${${test}_extra_SOURCES})
        target_link_libraries(${test} gtest gtest_main ${my_LINK_LIBS})
        break()
      endif()
    endforeach()

    set(FOUND FALSE)
    foreach( SUFFIX py rb sh )
      if(EXISTS ${PROJECT_SOURCE_DIR}/${test}.${SUFFIX})
        # Compiled test (possibly run via a helper script)
        # (Partly) interpreted test
        set(FOUND TRUE)
        add_wrapped_test(${test} ${PROJECT_SOURCE_DIR}/${test}.${SUFFIX} "${my_FIXTURES}")
        break()
      endif()
    endforeach()
    # Not-at-all-interpreted test
    if (NOT ${FOUND})
        add_wrapped_test(${test} ${test} "${my_FIXTURES}")
    endif()
  endforeach()
 endif()
endfunction()

#------------- subdirectories ---------------

# Keep gtest/cmake/internal_utils.cmake from forcing /MT, causing link errors on tests
set(gtest_force_shared_crt TRUE CACHE BOOL "let's use /MD, ok?")
add_subdirectory(gtest)
add_subdirectory(doc)
add_subdirectory(libLoam/c)
add_subdirectory(libPlasma/c)
add_subdirectory(libLoam/c++)
add_subdirectory(libPlasma/c++)
if (NOT WIN32)
# Not yet quite ported
add_subdirectory(libPlasma/bench)
add_subdirectory(libPlasma/ruby)
add_subdirectory(libPlasma/zeroconf)
# adding all noodoo2-related directories here
endif()

# add_subdirectory(samples)

#------------- expand @keywords@ ---------------

# Variables set correctly, in alphabetical order
#STRING(REGEX REPLACE "^/" "" noslash_prefix ${CMAKE_INSTALL_PREFIX})

# Variables set kludgily, in alphabetical order (FIXME)

# comment out all of this git stuff for now...
# GIT_VERSION
#GIT_VERSION="$_describe\\n$_git_line_2\\n$_git_line_3"
# exec_program(
#     "git"
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ARGS "describe" "--long"
#     OUTPUT_VARIABLE GIT_VERSION )
# STRING(REGEX REPLACE "^.*-" "" PACKAGE_SHA ${GIT_VERSION})
#
# ABI_VERSION
# Assume release versions are binary compatible if major and minor are the same.
# Assume development versions are always incompatible.  (Use SHA as ABI version then.)
# if (GIT_VERSION MATCHES "rel-")
#     set(ABI_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}")
# else()
#     set(ABI_VERSION "${PACKAGE_VERSION_PATCH} ${PACKAGE_SHA}")
# endif()

# ... and instead just do this:
set(ABI_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}")

set(UBUNTU_RELEASE "dopey")

# FIXME: don't configure things into the source directory if you can help it
foreach(f
    libLoam/c/ob-vers-gen.h
)
    configure_file (
      "${g-speak_SOURCE_DIR}/${f}.in"
      "${g-speak_SOURCE_DIR}/${f}"
      @ONLY
    )
endforeach()
# NOTE: here's the right way to do it - configure them into the binary directory
foreach(f
    libPlasma/ruby/gembuild/rubyPlasma.gemspec
    libPlasma/ruby/gembuild/build.sh
)
    configure_file (
      "${f}.in" "${f}"
      @ONLY
    )
endforeach()

#------------- helpers ---------------

if (ASAN OR (UNIX AND NOT APPLE))
  # Needed for running e.g. ruby or python that loads a native g-speak plugin
  # Install unconditionally on Linux because conditional debian packaging is... hard.
  install(
    FILES bld/cmake/asanpreload
    DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
endif()

# Convenience wrapper for setting environment variables when running e.g. meson.
# Assumes $G_SPEAK_HOME/bin is on PATH *or* debian alternatives are used to find it.
configure_file(bld/obenv.in obenv @ONLY)
install(
  FILES ${g-speak_BINARY_DIR}/obenv
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

#------------- final words ---------------

# This must be the last subdirectory added
add_subdirectory(bld/cmake)

#------------- Tell user what happened ---------------

MESSAGE("")
MESSAGE("----------------- Summary -----------------------------------")
MESSAGE("")
MESSAGE("CMAKE_BINARY_DIR:  ${CMAKE_BINARY_DIR} (directory cmake was run in)")
MESSAGE("")
MESSAGE("--------- -D Options from commandline -----------------------")
MESSAGE("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
MESSAGE("CMAKE_BUILD_TYPE:  ${CMAKE_BUILD_TYPE} (choices are Debug, Release, or RelWithDebInfo)")
MESSAGE("STRICT_WARNINGS:   ${STRICT_WARNINGS}")
MESSAGE("WERROR:            ${WERROR}")
MESSAGE("BUILD_TESTS:       ${BUILD_TESTS}")
MESSAGE("COVERAGE:          ${COVERAGE}; see https://gitlab.oblong.com/platform/docs/wikis/coverage-testing")
MESSAGE("STATIC_ONLY:       ${STATIC_ONLY}")
MESSAGE("GREENHOUSE:        ${GREENHOUSE}")
MESSAGE("ASAN:              ${ASAN}; see https://github.com/google/sanitizers/wiki/AddressSanitizer")
MESSAGE("TSAN:              ${TSAN}; see https://github.com/google/sanitizers/wiki/ThreadSanitizer")
MESSAGE("")
MESSAGE("--------- Environment vars ----------------------------------")
MESSAGE("EXTRA_CFLAGS:      $ENV{EXTRA_CFLAGS}")
MESSAGE("EXTRA_CXXFLAGS:    $ENV{EXTRA_CXXFLAGS}")
MESSAGE("")
MESSAGE("--------- Intermediate compiler flags -----------------------")
MESSAGE("OPTIMIZE_FLAGS:    ${OPTIMIZE_FLAGS}")
MESSAGE("ERROR_CFLAGS:      ${ERROR_CFLAGS}")
MESSAGE("ERROR_CXXFLAGS:    ${ERROR_CXXFLAGS}")
MESSAGE("SANITIZER_FLAGS:   ${SANITIZER_FLAGS}")
MESSAGE("COVERAGE_FLAGS:    ${COVERAGE_FLAGS}")
MESSAGE("VISIBILITY_FLAGS:  ${VISIBILITY_FLAGS}")
MESSAGE("")
MESSAGE("--------- Final global compiler flags -----------------------")
MESSAGE("CMAKE_C_FLAGS:     ${CMAKE_C_FLAGS}")
MESSAGE("CMAKE_CXX_FLAGS:   ${CMAKE_CXX_FLAGS}")
MESSAGE("GRAPHICSMAGICKPP_CFLAGS_STRING: ${GRAPHICSMAGICKPP_CFLAGS_STRING}")
MESSAGE("")
MESSAGE("")
