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

project('loadavgwatch', 'c', default_options : ['c_std=c99'])

executable_files = ['main.c']
library_files = ['loadavgwatch.c']
if target_machine.system() == 'linux'
    executable_files += 'main-linux.c'
    library_files += 'loadavgwatch-linux.c'
elif target_machine.system() == 'darwin'
    executable_files += 'main-darwin.c'
    library_files += 'loadavgwatch-darwin.c'
endif

lib = static_library('loadavgwatch', library_files)
executable('loadavgwatch', executable_files, link_with : lib)
