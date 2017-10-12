/**
 * Copyright (C) 2017 Jussi Judin
 *
 * This file is part of loadavgwatch.
 *
 * loadavgwatch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * loadavgwatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with loadavgwatch.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include "main-impl.h"
#include <sys/sysctl.h>

long get_ncpus(void)
{
    int mib[2] = {CTL_HW, HW_NCPU};
    size_t len = sizeof(mib) / sizeof(mib[0]);
    int ncpus;
    size_t cpus_size = sizeof(ncpus);
    int read_result = sysctl(mib, len, &ncpus, &cpus_size, NULL, 0);
    if (read_result == ENOMEM) {
        return 0;
    }
    if (read_result != 0) {
        return 0;
    }
    return ncpus;
}
