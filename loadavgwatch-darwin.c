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
#include "loadavgwatch-impl.h"
#include <sys/sysctl.h>

long loadavgwatch_impl_get_ncpus(void)
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

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state)
{
    float load;
    loadavgwatch_status status = loadavgwatch_impl_get_load_average(
        NULL, &load);
    if (status != LOADAVGWATCH_OK) {
        PRINT_LOG_MESSAGE(state->log_error, "Initial load reading failed!");
        return status;
    }
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_close(void* state)
{
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* state, float* out_loadavg)
{
    *out_loadavg = 1.0;
    int mib[2] = {CTL_VM, VM_LOADAVG};
    size_t len = sizeof(mib) / sizeof(mib[0]);
    struct loadavg load;
    size_t load_size = sizeof(load);
    int read_result = sysctl(mib, len, &load, &load_size, NULL, 0);
    if (read_result == ENOMEM) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    if (read_result != 0) {
        return LOADAVGWATCH_ERR_READ;
    }
    *out_loadavg = (float)load.ldavg[0] / load.fscale;
    return LOADAVGWATCH_OK;
}

const char* loadavgwatch_impl_get_system(void)
{
    return "darwin";
}
