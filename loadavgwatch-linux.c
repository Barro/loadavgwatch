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

/**
 * Parses Linux /proc/cpuinfo file for the number of CPUs
 */
static long get_ncpus_proc_cpuinfo(const char* path)
{
    FILE* cpuinfo_fp = fopen(path, "r");
    if (!cpuinfo_fp) {
        return -1;
    }
    char line_buffer[1024];
    long ncpus = 0;
    while (fgets(line_buffer, sizeof(line_buffer), cpuinfo_fp)) {
        char* processor_position = strstr(line_buffer, "processor");
        if (processor_position != line_buffer) {
            continue;
        }
        // Make sure that "processor" is the full word on the line:
        if (!(line_buffer[sizeof("processor") - 1] == ' '
              || line_buffer[sizeof("processor") - 1] == '\t'
              || line_buffer[sizeof("processor") - 1] == ':')) {
            continue;
        }
        char* colon_position = strstr(line_buffer, ":");
        if (colon_position == NULL) {
            continue;
        }
        // We have a line that has processor in the beginning and
        // colon somewhere in it. Register it as a new CPU.
        ncpus++;
    }
    fclose(cpuinfo_fp);
    return ncpus;
}

/**
 * Parses Linux /sys/devices/system/cpu/online file for the number of
 * CPUs
 */
static long get_ncpus_sys_devices(const char* path)
{
    FILE* online_cpus_fp = fopen(path, "r");
    if (online_cpus_fp == NULL) {
        return -1;
    }
    // 19369 is the theoretical length that CPU list can have if each
    // and every one of the maximum 4096 CPUs is listed
    // individually. That will never be the case, but let's just use
    // some stack, as this function should be only called once at
    // start-up.
    char cpumask_buffer[19370] = {0};
    size_t read_bytes = fread(
        cpumask_buffer, 1, sizeof(cpumask_buffer) - 1, online_cpus_fp);
    fclose(online_cpus_fp);
    if (read_bytes == 0) {
        return -1;
    }
    long ncpus = 0;
    char* saveptr = NULL;
    char* until_token = strtok_r(cpumask_buffer, ",", &saveptr);
    while (until_token != NULL) {
        char* dash_position = strstr(until_token, "-");
        if (dash_position != NULL) {
            unsigned long first_cpu;
            unsigned long last_cpu;
            if (sscanf(until_token, "%lu-%lu", &first_cpu, &last_cpu) != 2) {
                return -1;
            }
            ncpus += last_cpu - first_cpu + 1;
        } else {
            ncpus++;
        }
        until_token = strtok_r(NULL, ",", &saveptr);
    }
    return ncpus;
}

static long get_ncpus_sysconf(void)
{
#ifdef _SC_NPROCESSORS_ONLN
    long cpus_online = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus_online > 0) {
        return cpus_online;
    }
#endif // #ifdef _SC_NPROCESSORS_ONLN
#ifdef _SC_NPROCESSORS_CONF
    long cpus_configured = sysconf(_SC_NPROCESSORS_CONF);
    if (cpus_configured > 0) {
        return cpus_configured;
    }
#endif // #ifdef _SC_NPROCESSORS_CONF
    return -1;
}

long loadavgwatch_impl_get_ncpus(void)
{
    // It's possible that neither /proc/ nor /sys/ are fully
    // mounted. It can be the case when running inside a container or
    // other chroot mechanism.
    long ncpus_list[] = {
        get_ncpus_proc_cpuinfo("/proc/cpuinfo"),
        get_ncpus_sys_devices("/sys/devices/system/cpu/online"),
        get_ncpus_sysconf(),
    };

    long ncpus = -1;
    for (size_t i = 0; i < sizeof(ncpus_list) / sizeof(ncpus_list[0]); ++i) {
        if (ncpus_list[i] > ncpus) {
            ncpus = ncpus_list[i];
        }
    }
    return ncpus;
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

const char* loadavgwatch_impl_get_system(void)
{
    return "linux";
}
