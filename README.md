# loadavgwatch

Monitors the load average and runs scripts when the load average
exceeds or falls under pre-specified values.

## Usage

## Building

There are several build options that are mainly available for
situations where loadavgwatch is used as a dependency of a larger
build sequence.

### Bazel

```bash
bazel build :all  # Results in binary at bazel-bin/loadavgwatch
```

### Meson

```bash
meson build-meson
ninja -C build-meson  # Results in binary at build-meson/loadavgwatch
```

### Manual builds

This is a simple and small enough program so that it can be built
manually with any recent C compiler if the target system has the
required functionality. Build system is optional.

```bash
# GNU/Linux
cc --std=c99 -o loadavgwatch -I. main.c loadavgwatch.c loadavgwatch-linux.c
# OS X
cc --std=c99 -o loadavgwatch -I. main.c loadavgwatch.c loadavgwatch-darwin.c
# BSD
cc --std=c99 -o loadavgwatch -I. main.c loadavgwatch.c loadavgwatch-bsd.c
```

## Development [![Build Status](https://travis-ci.org/Barro/loadavgwatch.svg?branch=master)](https://travis-ci.org/Barro/loadavgwatch)


## License

loadavgwatch is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3 as
published by the Free Software Foundation.

loadavgwatch is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of GNU General Public License version 3 can be found from file
[COPYING](COPYING).
