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
    library_files += 'loadavgwatch-linux.c'
elif target_machine.system() == 'darwin'
    library_files += ['loadavgwatch-darwin.c']
elif target_machine.system() == 'freebsd'
    library_files += ['loadavgwatch-bsd.c']
endif

lib = static_library(
    'loadavgwatch',
    library_files,
    install : true,
    c_args : ['-Werror=pedantic'])
executable(
    'loadavgwatch',
    executable_files,
    install : true,
    link_with : lib,
    c_args : ['-Werror=pedantic'])

# Test related:
test('Parser tests',
     executable(
         'test-main-parsers',
         ['test-main-parsers.c'],
         c_args : ['-Werror=pedantic']))

# Installation information:
install_headers('loadavgwatch.h')
install_man('loadavgwatch.1')
conf_data = configuration_data()
conf_data.set('PACKAGE_VERSION', '0.0.0')
prefix = get_option('prefix')
conf_data.set('libdir', join_paths(prefix, get_option('libdir')))
conf_data.set('includedir', join_paths(prefix, get_option('includedir')))
configure_file(
    input : 'libloadavgwatch.pc.in',
    output : 'libloadavgwatch.pc',
    configuration : conf_data,
    install_dir : join_paths(prefix, get_option('libdir'), 'pkgconfig'))
