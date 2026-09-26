# Plasma tests come in two flavors:
# 1. a single program in c or c++ run by a ruby or shell script, or
# 2. ruby or shell scripts that run binaries already built elsewhere.
# The easiest way to express all this is to just have a list of things
# to build, and then a separate list of things to run,
# and rely on meson to build everything before running anything.
# See https://github.com/mesonbuild/meson/issues/1704

#---- Build binaries from C or C++ ----

plasma_c_t_incs = include_directories('..')

# Tests that don't use google test

plasma_c_t_sources = [
  'endian_test.c',
  'ilk-begotten.c',
  'ins-rep.c',
  'listcat_test.c',
  'list-search.c',
  'map-merge.c',
  'nonconformist.c',
  'pad_tests.c',
  'protein-saratoga.c',
  'rude_test.c',
  'rumpus-room.c',
  'simple-list.c',
  'simple-numerics.c',
  'slabu-list.c',
  'slabu-map.c',
  'slat.c',
  'slaw-preakness.c',
  'slumber.c',
  'slumcat.c',
  'slypes.c',
  'spew-multi.c',
  'test-boolean.c',
  'testcoerce.c',
  'test-interop.c',
  'test-map.c',
  'test_new_stuff.c',
  'test-pack.c',
  'test-path.c',
  'test-slaw-flush.c',
  'test-slaw-io.c',
  'test-string.c',
  'testvcoerce.c',
  'test-yaml.c',
  'tls-hostname-test.c',
  'various-types.c',
  'yaml-all-numeric.c',
  'yaml-options.c',
]

fs = import('fs')

foreach p : plasma_c_t_sources
   base = fs.replace_suffix(p, '')
   executable(
     base, p,
     c_args : c_flags + test_c_flags,
     cpp_args : c_flags + test_c_flags,
     link_with : [loam_c, plasma_c],
     dependencies : [plasma_c_deps],
     include_directories : [plasma_c_incs, plasma_c_t_incs],
   )
endforeach

# Tests that do use google test

plasma_c_gtest_sources = [
  'MiscSlawTest.cpp',
  'SlawDuplicatesTest.cpp',
]

foreach p : plasma_c_gtest_sources
   base = fs.replace_suffix(p, '')
   executable(
     base, p,
     cpp_args : c_flags + test_c_flags,
     link_with : [loam_c, plasma_c],
     dependencies : [plasma_c_deps, gtest_main_dep],
     include_directories : [plasma_c_incs, plasma_c_t_incs],
   )
endforeach

# And one that needs an extra library

foreach p : [ 'spew-test.c' ]
   base = fs.replace_suffix(p, '')
   executable(
     base, p,
     c_args : c_flags + test_c_flags,
     link_with : [loam_c, plasma_c],
     dependencies : [plasma_c_deps, icu_uc_dep, icu_i18n_dep],
     include_directories : [plasma_c_incs, plasma_c_t_incs],
   )
endforeach

#---- Things to run ----

plasma_c_ts = [
  'bug386.sh',
  'endian_test_big.sh',
  'endian_test_big-v1.sh',
  'endian_test_little.sh',
  'endian_test_little-v1.sh',
  'listcat_test_multithreaded.sh',
  'listcat_test.sh',
  'pad_tests.sh',
  'rumpus-room.sh',
  'slaw-preakness.sh',
  'slumber.sh',
  'spew-test.sh',
  'testcoerce.sh',
  'test-interop.sh',
  'testvcoerce.sh',
  'tls-hostname-test',
  'yet-another-yaml-test.sh',
]

# libPlasma/c/t/*.sh want srcdir
testenv = { 'srcdir' : meson.current_source_dir() }

foreach p : plasma_c_ts
  if p.contains('.')
     # Suffix?  It's a script.
     f = meson.current_source_dir() + '/' + p
  else
     # No suffix?  It's a compiled binary.
     f = meson.current_build_dir() + '/' + p
  endif

  base = fs.replace_suffix(p, '')
  test(base, yotest,
       args : ['run', 'local', f],
       is_parallel : false,
       suite: 'plasma',
       env : testenv)
endforeach
