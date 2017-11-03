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

/**
 * This file works on OS X and FreeBSD systems.
 */

#include <errno.h>
#include "loadavgwatch-impl.h"
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <stdlib.h>

typedef struct sysctl_mibs
{
    int loadavg[4];
    size_t loadavg_len;
} sysctl_mibs;

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
    sysctl_mibs* mibs = calloc(1, sizeof(sysctl_mibs));
    if (mibs == NULL) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    mibs->loadavg_len = sizeof(mibs->loadavg) / sizeof(mibs->loadavg[0]);
    int loadavg_mib_ret = sysctlnametomib(
        "vm.loadavg",
        mibs->loadavg,
        &mibs->loadavg_len);
    if (loadavg_mib_ret == ENOMEM) {
        free(mibs);
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    if (loadavg_mib_ret != 0) {
        free(mibs);
        return LOADAVGWATCH_ERR_INIT;
    }
    float load;
    loadavgwatch_status status = loadavgwatch_impl_get_load_average(
        mibs, &load);
    if (status != LOADAVGWATCH_OK) {
        PRINT_LOG_MESSAGE(state->log_error, "Initial load reading failed!");
        return status;
    }
    *out_impl_state = mibs;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_close(void* impl_state)
{
    free(impl_state);
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* state, float* out_loadavg)
{
    sysctl_mibs* mibs = (sysctl_mibs*)state;
    struct loadavg load;
    size_t load_size = sizeof(load);
    int read_result = sysctl(
        mibs->loadavg, mibs->loadavg_len, &load, &load_size, NULL, 0);
    if (read_result == ENOMEM) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    if (read_result != 0) {
        return LOADAVGWATCH_ERR_READ;
    }
    *out_loadavg = (float)load.ldavg[0] / load.fscale;
    return LOADAVGWATCH_OK;
}
