#!/bin/sh

set -xeu

TIMEOUT=$(which timeout 2>/dev/null || which gtimeout 2>/dev/null)

meson build-meson
ninja -C build-meson
"$TIMEOUT" 2 build-meson/loadavgwatch --help
"$TIMEOUT" 2 build-meson/loadavgwatch --version
"$TIMEOUT" 2 build-meson/loadavgwatch --timeout 0

bazel build :all
"$TIMEOUT" 2 bazel-bin/loadavgwatch --help
"$TIMEOUT" 2 bazel-bin/loadavgwatch --version
"$TIMEOUT" 2 bazel-bin/loadavgwatch --timeout 0