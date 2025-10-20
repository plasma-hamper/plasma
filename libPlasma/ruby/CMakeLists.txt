# /* (c) 2014 Oblong Industries */

cmake_minimum_required(VERSION 3.6.2)
project(rubyPlasma)

# Avoid cmake warnings on Mac
# if (POLICY CMP0042)
#   cmake_policy(SET CMP0042 NEW)
# endif ()

# execute_process(
#   COMMAND ${g-speak_SOURCE_DIR}/bld/cmake/get-ruby-include-dirs.sh
#   OUTPUT_VARIABLE RUBY_INCLUDE_DIRS
#   OUTPUT_STRIP_TRAILING_WHITESPACE
# )
# STRING(REPLACE "\n" ";" RUBY_INCLUDE_DIRS "${RUBY_INCLUDE_DIRS}")
# message("RUBY_INCLUDE_DIRS is ${RUBY_INCLUDE_DIRS}")
# include_directories(SYSTEM ${RUBY_INCLUDE_DIRS})
# execute_process(
#   COMMAND ${g-speak_SOURCE_DIR}/bld/cmake/get-ruby-libs.sh
#   OUTPUT_VARIABLE RUBY_LIBS
#   OUTPUT_STRIP_TRAILING_WHITESPACE
# )
# message("RUBY_LIBS is ${RUBY_LIBS}")

# get_property(inc_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
# message("inc_dirs = ${inc_dirs}")

# set(
#   rubyPlasma_SOURCES

#   rubyPlasma.c
# )

# list(APPEND rubyPlasma_LINK_LIBS Plasma ${RUBY_LIBS})

# #------------- cflags -----------

# ObCheckCFlags(CMAKE_C_FLAGS
#   -Wno-strict-prototypes   # ruby's RUBY_METHOD_FUNC blows chow.  Ignore warnings.
# )

# #------------- library -----------

# # FIXME: Generate shared library even if LIBRARY_TYPE is SHARED, else
# # ruby tests fail as is.
# add_library(rubyPlasma SHARED ${rubyPlasma_SOURCES})
# target_link_libraries(rubyPlasma ${rubyPlasma_LINK_LIBS})
# # Don't want name to start with lib
# set_target_properties(rubyPlasma PROPERTIES
#   PREFIX ""
#   INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib/ruby"
# )
# if(APPLE)
#   # Ruby on Mac OS X looks for .bundle, not .so
#   set_target_properties(rubyPlasma PROPERTIES SUFFIX ".bundle")
# endif()

# #------------- install -----------

# set(
#   install_headers

#   Pool.rb
#   Protein.rb
#   SlawInputFile.rb
#   slaw-not-tests.rb
#   SlawOutputFile.rb
#   Slaw.rb
# )

# install (
#   FILES ${install_headers}
#   DESTINATION "${CMAKE_INSTALL_LIBDIR}/ruby"
# )

# install (
#   TARGETS rubyPlasma
#   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/ruby"
#   ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/ruby"
# )

# if (MSVC)
#   install(
#     FILES ${rubyPlasma_BINARY_DIR}/${BIN_SUBDIR}/rubyPlasma.pdb
#     DESTINATION "bin"
#     CONFIGURATIONS Debug RelWithDebInfo
#   )
# endif()

# #----- But wait, there's more! ----------------------------------------

# project(PlasmaRubyTests)

# # Don't run delete-pools.wrap.rb, as it's too order-dependent, and fails if run first :-(

# set(
#   PlasmaRubyTests_TESTS

#   bug3787.wrap.rb
#   file-io-test.wrap.rb
#   hose-extension-tests.wrap.rb
#   json-tests.wrap.rb
#   pool-tests.wrap.rb
#   slaw-tests.wrap.rb
#   types-test.wrap.rb
# )

# # Configure and set permissions in two steps, sigh
# configure_file(rubyPlasmaWrapper.cmake.in ${CMAKE_BINARY_DIR}/rubyPlasmaWrapper @ONLY)
# file(COPY ${CMAKE_BINARY_DIR}/rubyPlasmaWrapper
#   DESTINATION ${PlasmaRubyTests_BINARY_DIR}
#   FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
# )
# # And save another copy in the source directory for the sake of the tutorials
# file(COPY ${CMAKE_BINARY_DIR}/rubyPlasmaWrapper
#   DESTINATION ${PlasmaRubyTests_SOURCE_DIR}
#   FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
# )
# set(PlasmaRubyTests_LINK_LIBS )
# generate_tests("${PlasmaRubyTests_TESTS}" "")

# set(
#   PlasmaRubyTests_NOINST_PROGRAMS

#   c-test-feeder
# )

# set(SUFFIX "c")
# foreach( test ${PlasmaRubyTests_NOINST_PROGRAMS} )
#    add_executable(${test} ${test}.${SUFFIX})
#    target_link_libraries(${test} Plasma)
# endforeach()

# #--------- subdirs -----------------

# add_subdirectory(gembuild)
