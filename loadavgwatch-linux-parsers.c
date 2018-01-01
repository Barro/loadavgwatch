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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif // #define _XOPEN_SOURCE

#include "loadavgwatch-impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static loadavgwatch_status _get_load_average_proc_loadavg(
    FILE* loadavg_fp, float* out_loadavg)
{
    char read_buffer[128];
    char* line_buffer = fgets(read_buffer, sizeof(read_buffer), loadavg_fp);
    if (line_buffer == NULL) {
        return LOADAVGWATCH_ERR_READ;
    }
    int read_result = sscanf(read_buffer, "%f", out_loadavg);
    if (read_result != 1) {
        return LOADAVGWATCH_ERR_PARSE;
    }
    return LOADAVGWATCH_OK;
}

static long _get_ncpus_proc_cpuinfo(FILE* cpuinfo_fp)
{
    char line_buffer[1024] = {0};
    long ncpus = 0;
    while (fgets(line_buffer, sizeof(line_buffer), cpuinfo_fp)) {
        // Check that word "processor" is in the beginning of the line:
        if (strncmp("processor", line_buffer, sizeof("processor") - 1) != 0) {
            continue;
        }
        // First check that there is a colon on the line:
        char* colon_position = strchr(line_buffer, ':');
        if (colon_position == NULL) {
            continue;
        }
        // Make sure that "processor" is the full word on the line:
        if (!(line_buffer[sizeof("processor") - 1] == ' '
              || line_buffer[sizeof("processor") - 1] == '\t'
              || line_buffer[sizeof("processor") - 1] == ':')) {
            continue;
        }
        // We have a line that has processor in the beginning and
        // colon somewhere in it. Register it as a new CPU.
        ncpus++;
    }
    return ncpus;
}

static long _get_ncpus_sys_devices(FILE* online_cpus_fp)
{
    // 19369 is the theoretical length that CPU list can have if each
    // and every one of the maximum 4096 CPUs is listed
    // individually. That will never be the case, but as this is a
    // lazy implementation and this function should be only called
    // once at start-up, one calloc() is not that bad.
    const size_t cpumask_buffer_size = 19370;
    char* cpumask_buffer = (char*)calloc(1, cpumask_buffer_size);
    if (cpumask_buffer == NULL) {
        return -1;
    }
    size_t read_bytes = fread(
        cpumask_buffer, 1, cpumask_buffer_size - 1, online_cpus_fp);
    if (read_bytes == 0) {
        free(cpumask_buffer);
        return -1;
    }
    long ncpus = 0;
    char* saveptr = NULL;
    char* until_token = strtok_r(cpumask_buffer, ",", &saveptr);
    while (until_token != NULL) {
        char* dash_position = strchr(until_token, '-');
        if (dash_position != NULL) {
            unsigned long first_cpu;
            unsigned long last_cpu;
            if (sscanf(until_token, "%lu-%lu", &first_cpu, &last_cpu) != 2) {
                free(cpumask_buffer);
                return -1;
            }
            ncpus += last_cpu - first_cpu + 1;
        } else {
            ncpus++;
        }
        until_token = strtok_r(NULL, ",", &saveptr);
    }
    free(cpumask_buffer);
    return ncpus;
}
