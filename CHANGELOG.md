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
