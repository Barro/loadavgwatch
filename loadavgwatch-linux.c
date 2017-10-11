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

#define _POSIX_C_SOURCE 199309L
// _POSIX_C_SOURCE disables some of the standard C99 functions on OS
// X. Define _C99_SOURCE to word around that issue.
#define _C99_SOURCE

#include "loadavgwatch-impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>

typedef struct _state_linux state_linux;

struct _state_linux
{
    FILE* loadavg_fp;
    loadavgwatch_status(*get_load_average)(
        state_linux* state, float* out_loadavg);
};

static loadavgwatch_status get_load_average_proc_loadavg(
    state_linux* state, float* out_loadavg)
{
    char read_buffer[128];
    char* line_buffer = fgets(read_buffer, sizeof(read_buffer), state->loadavg_fp);
    fseek(state->loadavg_fp, 0, SEEK_SET);
    if (line_buffer == NULL) {
        return LOADAVGWATCH_ERR_READ;
    }
    int read_result = sscanf(read_buffer, "%f", out_loadavg);
    if (read_result != 1) {
        return LOADAVGWATCH_ERR_PARSE;
    }
    return LOADAVGWATCH_OK;
}

static loadavgwatch_status get_load_average_sysinfo(
    state_linux* state, float* out_loadavg)
{
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return LOADAVGWATCH_ERR_READ;
    }
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state)
{
    state_linux* impl_state = calloc(1, sizeof(state_linux));
    if (impl_state == NULL) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }
    FILE* loadavg_fp = fopen("/proc/loadavg", "r");
    if (loadavg_fp == NULL) {
        PRINT_LOG_MESSAGE(
            state->log_warning,
            "Unable to open /proc/loadavg for reading! "
            "Falling back on sysinfo method.");
    } else {
        impl_state->loadavg_fp = loadavg_fp;
        impl_state->get_load_average = get_load_average_proc_loadavg;
        *out_impl_state = impl_state;
        return LOADAVGWATCH_OK;
    }

    float sysinfo_loadavg;
    loadavgwatch_status sysinfo_result = get_load_average_sysinfo(
        impl_state, &sysinfo_loadavg);
    if (sysinfo_result != LOADAVGWATCH_OK) {
        PRINT_LOG_MESSAGE(
            state->log_error,
            "Unable to use sysinfo load average method! "
            "No fallbacks available anymore!");
        free(impl_state);
        return sysinfo_result;
    }

    impl_state->get_load_average = get_load_average_sysinfo;
    *out_impl_state = impl_state;
    return sysinfo_result;
}

loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* impl_state, float* out_loadavg)
{
    state_linux* state = (state_linux*)impl_state;
    return state->get_load_average(state, out_loadavg);
}

loadavgwatch_status loadavgwatch_impl_close(void* impl_state)
{
    if (impl_state == NULL) {
        return LOADAVGWATCH_OK;
    }
    state_linux* state = (state_linux*)impl_state;
    if (state->loadavg_fp != NULL) {
        fclose(state->loadavg_fp);
    }
    memset(state, 0, sizeof(*state));
    free(state);
    return LOADAVGWATCH_OK;
}
