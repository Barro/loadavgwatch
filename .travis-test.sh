#!/bin/sh

set -xeu

TIMEOUT=$(which timeout 2>/dev/null || which gtimeout 2>/dev/null)

# TODO for some reason Meson build on OS X in Travis CI does not
# recognize fmemopen() function and fails.
if [[ "$TRAVIS_OS_NAME" != osx ]]; then
    meson build-meson
    ninja -C build-meson
    "$TIMEOUT" 2 build-meson/loadavgwatch --help
    "$TIMEOUT" 2 build-meson/loadavgwatch --version
    "$TIMEOUT" 2 build-meson/loadavgwatch --timeout 0
fi

bazel test --test_output=errors :all
"$TIMEOUT" 2 bazel-bin/loadavgwatch --help
"$TIMEOUT" 2 bazel-bin/loadavgwatch --version
"$TIMEOUT" 2 bazel-bin/loadavgwatch --timeout 0
