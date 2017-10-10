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
    name = "libloadavgwatch",
    srcs = ["loadavgwatch.c"],
    hdrs = ["loadavgwatch.h"],
)

cc_binary(
    name = "loadavgwatch",
    srcs = ["main.c", "main-linux.c", "main-impl.h"],
    deps = [":libloadavgwatch"],
)
