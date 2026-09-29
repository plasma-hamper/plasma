# Plasma tests come in two flavors:
# 1. a single program in c or c++ run by a ruby or shell script, or
# 2. ruby or shell scripts that run binaries already built elsewhere.
# The easiest way to express all this is to just have a list of things
# to build, and then a separate list of things to run,
# and rely on meson to build everything before running anything.
# See https://github.com/mesonbuild/meson/issues/1704

#---- Build binaries from C or C++ ----

plasma_c_test_incs = include_directories('..')

# Tests that don't use google test

plasma_c_test_sources = [
  'check-stepsize-behavior.c',
  'deposit-timestamp.c',
  'doppelganger.c',
  'empty-gang.c',
  'many_creates.c',
  'null_test.c',
  'participate_create.c',
  'perf_test.c',
  'pingpong_test.c',
  'pool-log-test.c',
  'random-access-test.c',
  'rewind_test.c',
  'seek_test.c',
  'test-await-index.c',
  'test-bigger.c',
  'test-info.c',
  'test-multi-await.c',
  'wrap_test.c',
  'zombie.c',
]

fs = import('fs')

foreach p : plasma_c_test_sources
   base = fs.replace_suffix(p, '')
   executable(
     base, p,
     c_args : c_flags + test_c_flags,
     cpp_args : c_flags + test_c_flags,
     link_with : [loam_c, plasma_c],
     dependencies : [plasma_c_deps],
     include_directories : [plasma_c_incs, plasma_c_test_incs],
   )
endforeach

# Tests that do use google test

plasma_c_gtest_sources = [
  'MiscPoolTest.cpp',
  'RecentServerOnly.cpp',
  'TocPoolTest.cpp',
  'TocUnitTest.cpp',
]

foreach p : plasma_c_gtest_sources
   base = fs.replace_suffix(p, '')
   executable(
     base, p,
     cpp_args : c_flags + test_c_flags,
     link_with : [loam_c, plasma_c],
     dependencies : [plasma_c_deps, gtest_main_dep],
     include_directories : [plasma_c_incs, plasma_c_test_incs],
   )
endforeach

#---- Things to run ----

rigs = ['local', 'tcp', 'tcpo', 'tcps']

plasma_c_tests = [
  'await_test.sh',
  'bad_pool_name.sh',
  'bad_type.sh',
  'check-stepsize-behavior.sh',
  'create_existing_pool.sh',
  'deposit-timestamp.sh',
  'empty-gang.sh',
  'index_tests.sh',
  'list_pools.sh',
  'many_creates.sh',
  'MiscPoolTest',
  'null_pool_name.sh',
  'null_test.sh',
  'participate_create.sh',
  'pingpong_test.sh',
  'pool-log-test.sh',
  'pool_tests.sh',
  'protein_tests.sh',
  'random-access-test.sh',
  'RecentServerOnly.sh',
  'resize-stress.rb',
  'rewind_test.sh',
  'seek_test.sh',
  'slashed-list-pools.rb',
  'slashed-pool-name.rb',
  'stress_tests.sh',
  'TocPoolTest',
  'TocUnitTest',
  'tcp_pool_name.sh',
  'test-await-index.sh',
  'test-bigger.sh',
  'test-info.sh',
  'test-multi-await.sh',
  'test-p-stop.sh',
  'test-zap.sh',
  'too_big_pool.sh',
  'too_long_pool_name.sh',
  'too_small_pool.sh',
  'wrap_test.sh',
  'zombie.rb',
]

# libPlasma/c/tests/avoid-fallback.rb wants srcdir
testenv = { 'srcdir' : meson.current_source_dir() }

# Things that run slowly
# FIXME: speed these up
slowtests = [
  'MiscPoolTest-local',
  'MiscPoolTest-tcp',
  'MiscPoolTest-tcpo',
  'MiscPoolTest-tcps',
  'RecentServerOnly-tcp',
  'RecentServerOnly-tcpo',
  'RecentServerOnly-tcps',
  'await_test-tcps',
  'many_creates-tcp',
  'many_creates-tcpo',
  'many_creates-tcps',
  'random-access-test-tcpo',
  'random-access-test-tcps',
  'resize-stress-tcp',
  'resize-stress-tcpo',
  'resize-stress-tcps',
  'stress_tests-local',
  'stress_tests-tcp',
  'stress_tests-tcpo',
  'stress_tests-tcps',
]
fasttimeout = 30
slowtimeout = 200

foreach p : plasma_c_tests
   base = fs.replace_suffix(p, '')
   foreach rig : rigs
     if ((base + '-' + rig) in slowtests)
        if skipslowtests == 'yes'
           continue
        endif
        timeout = slowtimeout
        suite = 'plasma-slow'
     else
        timeout = fasttimeout
        suite = 'plasma'
     endif

     if p.contains('.')
        # Suffix?  It's a script.
        f = meson.current_source_dir() + '/' + p
     else
        # No suffix?  It's a compiled binary.
        f = meson.current_build_dir() + '/' + p
     endif

     test(base + '-' + rig, yotest, args : ['run', rig, f], is_parallel : false, env : testenv,
          suite: suite,
          timeout: timeout)
   endforeach
endforeach

# ipv6_pool_name reaches the server by address rather than by name, so it
# wants the 'tcp' rig alone: the TLS rigs' certificate names 'localhost',
# and would rightly be refused for '::1'.  Hence its own test() rather
# than a line in plasma_c_tests.
test('ipv6_pool_name-tcp', yotest,
     args : ['run', 'tcp', meson.current_source_dir() + '/ipv6_pool_name.sh'],
     is_parallel : false, env : testenv,
     suite : 'plasma', timeout : fasttimeout)
