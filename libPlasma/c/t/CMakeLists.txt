# /* (c)  oblong industries */

project(PlasmaT C CXX)

# Tests (if there's a wrapper with same base name, just give the base name)
set(
  PlasmaT_TESTS

  bug386.sh
  endian_test_big.sh
  endian_test_big-v1.sh
  endian_test_little.sh
  endian_test_little-v1.sh
  ilk-begotten
  ins-rep
  listcat_test
  listcat_test_multithreaded.sh
  list-search
  map-merge
  MiscSlawTest
  nonconformist
  pad_tests
  protein-saratoga
  rude_test
  rumpus-room
  simple-list
  simple-numerics
  slabu-list
  slabu-map
  slat
  SlawDuplicatesTest
  slaw-preakness
  slumber
  slumcat
  slypes
  spew-multi
  test-spew-ex
  test_new_stuff
  test-boolean
  testcoerce
  test-interop
  test-map
  test-pack
  test-path
  test-slaw-flush
  test-slaw-io
  test-string
  testvcoerce
  test-yaml
  various-types
  yaml-all-numeric
  yaml-options
  yet-another-yaml-test.sh
)

set(PlasmaT_LINK_LIBS Plasma ${Plasma_LINK_LIBS})
generate_tests("${PlasmaT_TESTS}" "${PlasmaT_LINK_LIBS}")

# spew-test depends on ICU, so only build it when ICU is available.
if(ICU_LIBRARIES AND BUILD_TESTS)
  set(SpewTest_LINK_LIBS ${PlasmaT_LINK_LIBS} ${ICU_LIBRARIES})
  generate_tests("spew-test" "${SpewTest_LINK_LIBS}")
endif()

# Test programs needed by the scripts in the above list (and not already
# inferred by generate_tests).  Usually only need to do this if the
# tests are scripts that rely on an executable that doesn't have same
# basename.
set(
  PlasmaT_noinst_PROGRAMS

  endian_test
)

foreach (prog ${PlasmaT_noinst_PROGRAMS})
  add_executable(${prog} ${prog}.c)
  target_link_libraries(${prog} Plasma Loam)
endforeach(prog)
