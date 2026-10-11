# Kogasatopia Metamod 1.12 parser backports

Branch: `kogasa-map-parsers`.

This branch starts at the official stable tag **1.12.0.1227**
(`75dd7b26fe92e535a11f0318ead73c3e05130d0b`). It deliberately retains the
classic SourceHook ABI required by the server's existing extensions.

The only runtime source change is `core/sourcehook/sh_memory.h`, backporting:

- [Upstream PR #127](https://github.com/alliedmodders/metamod-source/pull/127),
  commit `2fe0c3684523e90f71687feee55944348e944cff`: line-based Linux
  `GetPageBits()` parsing.
- [Upstream PR #152](https://github.com/alliedmodders/metamod-source/pull/152),
  commits `f2d9d3e9e532d0211b5bb398aec0bff0fbe0aada` and
  `bc36e072a194a332b97bfde1a34355d76c4aee32`: matching line-based
  `ModuleInMemory()` parsing and its upstream cleanup.

These replace formatted scanning and byte-by-byte line skipping with
`getline()` and `strtoul()`. Memory permissions are still read freshly from
the kernel; there is no stale permission cache, blind RWX change, or hook ABI
change. Non-Linux implementations are unchanged. Original upstream authors
and commit references are preserved.

## Build and verify

Initialize the pinned submodules and provide an existing TF2 HL2SDK:

```sh
git submodule update --init --recursive
mkdir build-kogasa
cd build-kogasa
CC=clang-19 CXX=clang++-19 CXXFLAGS=-Wno-error=unknown-warning-option \
    python3 ../configure.py --enable-optimize --enable-debug --enable-tests \
    --sdks=tf2 --targets=x86 --hl2sdk-root=/path/to/hl2sdk-root
ambuild -j 1
./core/sourcehook/test/test_sourcehook/linux-x86/test_sourcehook
cd ..
CXX=clang++-19 bash tools/kogasa/run_map_parser_test.sh
```

The warning exception only accommodates a Clang-22-specific warning flag in
the SDK when using Clang 19; other warnings remain errors. It does not modify
the SDK or suppress runtime diagnostics. Use one worker for bounded memory
and CPU use. The build identifies itself by the fork commit rather than
pretending to be the unchanged official release.

The added Linux regression test checks every mapping's boundary permissions,
protection changes, no-access mappings, split ranges, writable conversion,
unmapping and null-address rejection on both x86 and x86-64. Run the existing
SourceHook lifecycle tests as well.

## Deployment boundary

Install the official 1.12.0.1227 loader/other binaries, then replace only the
TF2 x86 `addons/metamod/bin/metamod.2.tf2.so` with this branch's optimized
build. Stop all processes sharing that installation before replacing files.
Keep verified rollback copies. Do not overwrite `metamod.vdf`,
`metaplugins.ini`, SourceMod configuration, custom plugins or third-party
extensions. Validate startup on a test server before the main server.

The backports target hook attachment/removal overhead. They are not a claim
that every tickrate drop has the same cause or that the full server will be
four times faster.
