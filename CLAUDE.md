# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

The Animation Compression Library (ACL) is a **header-only C++11** library that compresses and decompresses animation data (transform tracks and scalar tracks) for games and realtime apps. All library code lives in `includes/acl/`; there is nothing to link. The compiled code is the unit tests (`tests/`), the `acl_compressor` tool (regression tests and statistics), and the `acl_decompressor` tool (benchmarks). Code must stay valid C++11 (CI also tests 14/17/20).

ACL uses [Realtime Math](https://github.com/nfrechette/rtm) (RTM, `external/rtm`) for all SIMD math, and [sjson-cpp](https://github.com/nfrechette/sjson-cpp) (`external/sjson-cpp`) to read and write clip files. Both are submodules.

## Build & test

Everything goes through `make.py` (wraps CMake + CTest). Run `git submodule update --init` first (RTM, sjson-cpp, Catch2, Google Benchmark in `external/`). Output goes to `./build`.

```bash
python3 make.py                      # generate project files only
python3 make.py -build -unit_test    # build + run all unit tests (Release by default)
python3 make.py -build -unit_test -config Debug
python3 make.py -unit_test -tests_matching "bitset"   # ctest --tests-regex; tests are discovered per Catch2 TEST_CASE
python3 make.py -build -regression_test               # compress + decompress the regression clips with every config
python3 make.py -clean -build        # clean rebuild
python3 make.py -build -bench        # build the decompression benchmarks
python3 make.py -run_bench           # run them (needs the decompression data in test_data/)
python3 make.py -analyze             # clang static analyzer on the public headers (clang only)
```

Useful variants: `-compiler {osx,clang23,gcc15,vs2026,android,ios,emscripten,...}`, `-cpu {x64,x86,armv7,arm64,arm64ec,riscv64,...}`, `-cpp_version {11,14,17,20}`, `-avx`, `-avx2`, `-pop` (POPCNT), `-nosimd` (RTM scalar fallback path), `-nosjson` (no clip IO), `-fast_math` (compiles the include validation files with fast math, to verify that the public headers compile without warnings in that mode). Changing these flags regenerates CMake; use `-clean` if the cache seems stale. On Android, `-unit_test` and `-regression_test` run on the device with `adb`. iOS builds need manual code signing in Xcode.

Regression tests (`-regression_test`) use `test_data/acl_regression_tests_v3.zip`; `make.py` downloads it when it is not there. Each clip is tested against each config in `test_data/configs/`. When a test fails, the script prints the `acl_compressor` command line that reproduces the failure. See `test_data/README.md` and `tools/acl_compressor/README.md`.

Builds use `-Wall -Wextra -Werror` (GCC/Clang) and `/W4 /WX` (MSVC) -- warnings break the build. Tests define `ACL_ON_ASSERT_THROW` (and `RTM_ON_ASSERT_THROW`, `SJSON_CPP_ON_ASSERT_THROW`) so asserts can be caught.

`tests/validate_includes` generates one `.cpp` per public header (`includes/acl/*.h`, `compression/*.h`, `core/*.h`, `decompression/*.h`, `decompression/database/*.h`) to verify each header compiles standalone -- every public header must include everything it uses.

## Architecture & conventions

- **Layout**: `core/` holds types shared by compression and decompression (track types, `compressed_tracks`, `compressed_database`, allocators, error handling). `compression/` holds the raw `track`/`track_array` types, `compress_track_list`, settings, and pre-processing. `decompression/` holds `decompression_context` and the database streamer. `io/` reads and writes `.acl.sjson` clip files. `math/` holds packing helpers on top of RTM.
- **Implementation details**: put code that is not public API in the nested `acl_impl` namespace and/or in a header under an `impl/` directory. Client code must not use either. A public header often ends with `#include "acl/.../impl/<name>.impl.h"` that holds the function bodies.
- **Header skeleton**: every header starts with `#pragma once` and the MIT license banner, then wraps content in `ACL_IMPL_FILE_PRAGMA_PUSH`/`POP` and `namespace acl { ACL_IMPL_VERSION_NAMESPACE_BEGIN ... ACL_IMPL_VERSION_NAMESPACE_END }`. New types should also be forward declared in the `fwd.h` of their directory.
- **Type naming suffixes**: an enum name ends with its size in bits (`track_type8`, `rotation_format8`, `compressed_tracks_version16`). Math types follow RTM (`vector4f`, `quatf`, `vqvf`).
- **Two track families**: transform tracks (`vqvf`) and scalar tracks (`float1f`..`float4f`, `vector4f`) have separate code paths. Files carry a `.transform.h` or `.scalar.h` suffix (for example `compress.transform.impl.h`, `decompression.scalar.h`). A change to shared behavior must stay consistent across both paths.
- **Decompression settings**: decompression is specialized at compile time through a `decompression_settings` struct of `static constexpr` functions (`is_track_type_supported`, `is_rotation_format_supported`, `are_extended_samples_supported`, ...). The optimizer removes the code for a feature that a setting turns off. When you add a feature to decompression, add a setting for it and keep the default variants (`default_transform_decompression_settings`, ...) as small as possible.
- **Compressed data versions**: `core/compressed_tracks_version.h` lists each binary format version. These values are serialized: do not change one after it is assigned. A change to the compressed format adds a new `vXX_YY_99_N` entry and updates `latest`. Decompression must still read each supported older version (see `decompression_version_selector.h`).
- **Asserts** (`core/error.h`) are stripped by default; `ACL_ON_ASSERT_ABORT`, `ACL_ON_ASSERT_THROW`, or `ACL_ON_ASSERT_CUSTOM` enable them. Functions that can fail return an `error_result`.
- **Comments**: use `//` comments. A `//////` banner line goes above each public function, type, and enum value that has a description.
- **Memory**: all allocations go through an `iallocator` (`allocate_type`, `deallocate_type_array`, ...). Do not use `new`, `delete`, or `malloc` directly.

## Tests

Tests are Catch2 (`tests/sources/`), grouped by area (`core/`, `compression/`, `io/`, `math/`) with tags such as `[core][utils]`. The IO tests are inside `ACL_IMPL_ENABLE_IO_UNIT_TESTS`. Platform-specific runners live in `tests/main_{generic,android,ios,emscripten}`. Unit tests do not cover the full compression pipeline: use `-regression_test` for that.

## Communication style

Write in ASD-STE100 (Simplified Technical English). This applies to what you say to the user, to
commit messages, to pull requests, and to comments and documents in the tree. Identifiers in code keep their names;
the specification controls prose only.

- **One word, one meaning, one part of speech.** Do not use a noun as a verb. Write "measure the
  function", not "benchmark the function".
- **Use the approved word when there is one.** Write "start", not "kick off"; "remove", not "strip
  out"; "let", not "permit". The technical nouns of this tree -- track, sample, clip, segment,
  bit rate, quantization, range reduction -- are technical names and are approved by that rule.
- **Active voice, and name the agent.** Write "the function sets the bit rate", not "the bit rate
  is set". Use the passive voice only when the agent is not known or is not important.
- **Short sentences.** Maximum 20 words in an instruction, 25 in a description. Write one
  instruction in one sentence.
- **Do not use the -ing form as a noun or as an adjective.** Write "the code that loads", not
  "the loading code"; write "to clear the track", not "clearing the track".
- **Keep the articles and the helper words.** Write "the error on the output track", not "error on
  output track". Text that drops them is not faster to read.
- **Present tense, no figures of speech, and no words that carry an opinion.** A number is
  "5% lower", not "disappointing".
- **No Unicode characters** in comments, string literals, documents, commit messages, or pull
  requests. Use only ASCII. For example, use `<->` instead of `↔`, `--` instead of `—`, `->`
  instead of `→`. STE does not permit a Unicode dash either.

## Commit messages and pull requests

Commits and pull requests use the
[Angular commit message format](https://github.com/angular/angular/blob/main/CONTRIBUTING.md#commit).
No tool enforces the format, so check each message yourself. The main branch is `develop`.

```
<type>(<scope>): <summary>

<body>

<footer>
```

- **Header** (required): `<type>(<scope>): <summary>`. The scope is optional.
  - `type` is one of: `build`, `ci`, `docs`, `feat`, `fix`, `perf`, `refactor`, `test`. This repo
    also uses `chore` (for example `chore(ci): ...`) and `style` (for a change with no effect on
    behavior, such as a rename or a move).
  - `scope` names the area of the change. Scopes in the history include `compression`,
    `decompression`, `core`, `io`, `math`, `tests`, `tools`, `bench`, and `ci`.
  - `summary` uses the imperative, present tense ("add", not "added" or "adds"). Do not capitalize
    the first letter. Do not put a period at the end.
- **Body**: use the imperative, present tense. Explain the motivation for the change. Compare the
  new behavior with the previous behavior when that helps.
- **Footer**: put breaking changes (`BREAKING CHANGE: <description>`), deprecations
  (`DEPRECATED: <description>`), and issue references (`Fixes #<number>`) here.

A pull request is merged with a merge commit. The pull request title becomes the header of that
merge commit, with the pull request number added (for example `feat: add support for arm64ec (#541)`).
Thus the pull request title must also use the `<type>(<scope>): <summary>` format, and each commit
in the pull request must use it too. The pull request description follows the
body rules above. Record notable changes in `CHANGELOG.md`.
