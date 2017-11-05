# Copyright (C) 2017 Jussi Judin
#
# This file is part of loadavgwatch.
#
# loadavgwatch is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# loadavgwatch is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with loadavgwatch.  If not, see <http://www.gnu.org/licenses/>.

cc_library(
    name = "libloadavgwatch_impl",
    srcs = ["loadavgwatch.c", "loadavgwatch-impl.h"] + select({
         ":linux_mode": ["loadavgwatch-linux.c"],
         ":darwin_mode": ["loadavgwatch-darwin.c", "loadavgwatch-sysctl.c"],
         ":freebsd_mode": ["loadavgwatch-bsd.c", "loadavgwatch-sysctl.c"],
    }),
    hdrs = ["loadavgwatch.h"],
    copts = ["--std=c99"],
    visibility = ["//visibility:private"],
    licenses = ["reciprocal"],
)
cc_inc_library(
    name = "libloadavgwatch",
    hdrs = ["loadavgwatch.h"],
    deps = [":libloadavgwatch_impl"],
)
cc_binary(
    name = "loadavgwatch",
    srcs = ["main.c"],
    deps = [":libloadavgwatch"],
    copts = ["--std=c99"],
    licenses = ["reciprocal"],
)
config_setting(
    name = "linux_mode",
    values = { "cpu": "k8" }
)
config_setting(
    name = "darwin_mode",
    values = { "cpu": "darwin" }
)
config_setting(
    name = "freebsd_mode",
    values = { "cpu": "freebsd" }
)
