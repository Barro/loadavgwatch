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

default: build-make/loadavgwatch

build-make/:
	mkdir build-make

build-make/loadavgwatch: build-make/ loadavgwatch.c main.c
	$(CC) -g3 -I. --std=c99 -o build-make/loadavgwatch main.c loadavgwatch.c
