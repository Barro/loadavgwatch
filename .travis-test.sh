#!/bin/sh

set -xeu

TIMEOUT=$(which timeout 2>/dev/null || which gtimeout 2>/dev/null)

WORKDIR=$(mktemp -d)
cleanup() {
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

test_installation()
{
    man loadavgwatch > /dev/null
    "$TIMEOUT" 2 loadavgwatch --version > /dev/null
    echo "#include <loadavgwatch.h>" > "$WORKDIR"/header.c
    echo "void test_lib(void) { loadavgwatch_open(NULL); }" >> "$WORKDIR"/header.c
    cc -shared -c $(pkg-config --cflags --libs libloadavgwatch) "$WORKDIR"/header.c
}

# TODO for some reason Meson build on OS X in Travis CI does not
# recognize fmemopen() function and fails.
if [ "$TRAVIS_OS_NAME" != osx ]; then
    meson build-meson
    ninja -C build-meson
    "$TIMEOUT" 2 build-meson/loadavgwatch --help
    "$TIMEOUT" 2 build-meson/loadavgwatch --version
    "$TIMEOUT" 2 build-meson/loadavgwatch --timeout 0

    sudo ninja -C build-meson install
    test_installation
fi

bazel test --test_output=errors :all
"$TIMEOUT" 2 bazel-bin/loadavgwatch --help
"$TIMEOUT" 2 bazel-bin/loadavgwatch --version
"$TIMEOUT" 2 bazel-bin/loadavgwatch --timeout 0
