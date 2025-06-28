* Docker build [#3](https://github.com/plasma-hamper/plasma/pull/3)

* Documentation fixes/improvements
  [#4](https://github.com/plasma-hamper/plasma/pull/4)
  [#6](https://github.com/plasma-hamper/plasma/pull/6)
  [#9](https://github.com/plasma-hamper/plasma/pull/9)

* Fix infinite loop bug in `ob_search_path()` when `path` argument is
  an empty string [#5](https://github.com/plasma-hamper/plasma/pull/5)

* Improve source code formatting
  [#7](https://github.com/plasma-hamper/plasma/pull/7)
  [#10](https://github.com/plasma-hamper/plasma/pull/10)

* Added new retort `OB_PARSE_ERROR`, and return it from
  `ob_strptime()` when the input string is malformed.  (Previously,
  `OB_OK` was being returned, even when the string could not be
  parsed.)  Also, a general rewrite/cleanup/simplification of
  `ob_strptime()`.  Added additional tests for `ob_strptime()`.
  [#8](https://github.com/plasma-hamper/plasma/pull/8)
