## Plasma 5.6.0 - released 2025-09-05

* Docker build [#3](https://github.com/plasma-hamper/plasma/pull/3)

* Documentation fixes/improvements
  [#4](https://github.com/plasma-hamper/plasma/pull/4)
  [#6](https://github.com/plasma-hamper/plasma/pull/6)
  [#9](https://github.com/plasma-hamper/plasma/pull/9)
  [#15](https://github.com/plasma-hamper/plasma/pull/15)

* Fix infinite loop bug in `ob_search_path()` when `path` argument is
  an empty string [#5](https://github.com/plasma-hamper/plasma/pull/5)

* Improve source code formatting
  [#7](https://github.com/plasma-hamper/plasma/pull/7)
  [#10](https://github.com/plasma-hamper/plasma/pull/10)
  [#13](https://github.com/plasma-hamper/plasma/pull/13)

* Added new retort `OB_PARSE_ERROR`, and return it from
  `ob_strptime()` when the input string is malformed.  (Previously,
  `OB_OK` was being returned, even when the string could not be
  parsed.)  Also, a general rewrite/cleanup/simplification of
  `ob_strptime()`.  Added additional tests for `ob_strptime()`.
  [#8](https://github.com/plasma-hamper/plasma/pull/8)

* Build configuration improvements
  [#11](https://github.com/plasma-hamper/plasma/pull/11)

  - Add CMake options for legacy build support:
    - `PLASMA_LEGACY_MODE`: Forces C++11 and disables modern features
    - `PLASMA_ENABLE_TCP_OPTIMIZATIONS`: Controls TCP buffer/timeout settings
    - `PLASMA_ENABLE_LARGE_TRANSFER_LOGGING`: Controls transfer logging

  - Add automatic legacy mode detection:
    - GCC < 5.0 or Clang < 3.4 triggers legacy mode
    - `PLASMA_LEGACY_BUILD` environment variable forces legacy mode

  - Add `plasma_config.h.in` for feature detection:
    - Detects `TCP_USER_TIMEOUT`, `TCP_KEEPINTVL`, `TCP_KEEPIDLE`
    - Detects `std::shuffle` and `std::mt19937` availability
    - Configures buffer sizes (1MB legacy, 16MB modern)
    - Configures timeouts (1min legacy, 5min modern)

  - Make TCP optimizations conditional:
    - Socket buffer settings only applied if enabled
    - TCP timeout features check for OS support
    - All optimizations wrapped in `PLASMA_ENABLE_TCP_OPTIMIZATIONS`

  - Make C++ modernizations conditional:
    - `std::shuffle` usage checks `HAVE_STD_SHUFFLE`
    - Falls back to `std::random_shuffle` in legacy mode
    - `compat.h` respects `PLASMA_LEGACY_BUILD` setting

  - Make logging conditional:
    - Large transfer logging checks `PLASMA_ENABLE_LARGE_TRANSFER_LOGGING`
    - Uses configurable `PLASMA_LARGE_TRANSFER_THRESHOLD` (1MB)

* Fix emscripten build.  Also, just generally be more correct in which
  header files we include in order to get `struct timeval`.
  [#2](https://github.com/plasma-hamper/plasma/pull/2)

* Allocate log codes and retorts for Haskell binding.
  [#12](https://github.com/plasma-hamper/plasma/pull/12)

* Change `slaw_spew_overview()` to display rude data as both hex and
  ASCII, rather than just hex.  This is inspired by the format used by
  the `hd(1)` command.  Additionally, a new libLoam function,
  `ob_fmt_hex_line()` has been added, which implements this
  functionality.
  [#14](https://github.com/plasma-hamper/plasma/pull/14)

* Fix up various issues with pkg-config files, which were preventing
  them from working properly with hs-plasma.  Most notably, some paths
  were the empty string, which caused problems.
  [#16](https://github.com/plasma-hamper/plasma/pull/16)

* Replace "g-speak" with "plasma" in user-visible messages.  This
  doesn't change symbols in the API that contain "gspeak", such as
  `OB_VERSION_OF_GSPEAK` in `ob-vers.h` or `Version_Of::Gspeak` in
  `ObInfo.h`.
  [#16](https://github.com/plasma-hamper/plasma/pull/16)

* Hardcode version number in `CMakeLists.txt`, instead of trying to
  determine it from tag names.
  [#16](https://github.com/plasma-hamper/plasma/pull/16)

## Plasma 5.6.1 - development version

* Fix cmake std::shuffle detection when compiler defaults to < C++-11.
  [#17](https://github.com/plasma-hamper/plasma/pull/17)

* Remove `Boost.System` from `find_package(Boost)`.
  [#18](https://github.com/plasma-hamper/plasma/pull/18)

* Update `cmake_minimum_version` in subprojects to 3.6.2 to match
  top-level `CMakeLists.txt`.
  [#18](https://github.com/plasma-hamper/plasma/pull/18)

* Change prototype of `slaw_string_is_valid_utf8()` to
  accept a `bslaw`, since the argument is constant.
  [#19](https://github.com/plasma-hamper/plasma/pull/19)

* Fix small memory leak on non-glibc (effectively, non-Linux)
  platforms.
  [#20](https://github.com/plasma-hamper/plasma/pull/20)

* Replace uses of `sprintf()` with `snprintf()` instead.
  [#22](https://github.com/plasma-hamper/plasma/pull/22)

* Make most dependencies optional, and change what is built accordingly.
  Allow building the C libraries without the C++ libraries.
  [#23](https://github.com/plasma-hamper/plasma/pull/23)

* Use `GNUInstallDirs` in build system.  (Apparently this had been used
  in the Oblong code base, but was removed when Plasma was extracted.)
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Restore `CMAKE_INSTALL_FULL_LIBDIR` in `libLoam.pc` LIBS flags.  This
  had been removed because it was empty and was causing problems, but
  re-enabling `GNUInstallDirs` makes it possible to restore this.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Update `cmake_minimum_required` to `VERSION 3.12...4.2`, and fix
  warnings (and other issues) caused by newer CMake versions.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Remove undefined `${SHELL}` from `add_wrapped_test` command.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Add `YT_ONLY_FIXTURES` env var to yotest to restrict test fixture
  list.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Skip tcps/tcpo test fixtures when OpenSSL is not found.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Add `TESTS.md`, a comprehensive guide to running the test suite.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* When running CMake, summarize which optional libraries were found.
  [#24](https://github.com/plasma-hamper/plasma/pull/24)

* Fix a bug in getting string descriptions for retorts.
  [#25](https://github.com/plasma-hamper/plasma/pull/25)

* Update Ruby test code to work with minitest gem ≥ 5.0.
  [#25](https://github.com/plasma-hamper/plasma/pull/25)

* Fix bugs in `spew-test`, `aptest3`, and `diffGitStatusTest`.
  [#25](https://github.com/plasma-hamper/plasma/pull/25)

* Add functions `slaw_spew_overview_ex()`,
  `slaw_spew_overview_to_string_ex()`, and
  `slaw_spew_overview_to_func()` to C Plasma, along with flags
  `SLAW_SPEW_FLAG_REL_OFF`, `SLAW_SPEW_FLAG_RUDE_ASCII`, and
  `SLAW_SPEW_FLAG_ESCAPE_STRINGS`.  These now allow more control over
  spew output.
  [#26](https://github.com/plasma-hamper/plasma/pull/26)

* Add class `Slaw::SpewOptions` to C++ Plasma.  Add overloaded
  `Spew()` methods which take `SpewOptions` as a second argument.
  This exposes the new C Plasma capabilities to the C++ API.
  [#26](https://github.com/plasma-hamper/plasma/pull/26)

* Add `SpewToString()` methods to C++ Plasma.  This makes it easier to
  spew directly to a `Str`.
  [#26](https://github.com/plasma-hamper/plasma/pull/26)
