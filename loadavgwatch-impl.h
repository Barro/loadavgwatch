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

#ifndef LOADAVGWATCH_IMPL_H
#define LOADAVGWATCH_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

#include "loadavgwatch.h"
#include <stdio.h>
#include <time.h>

#define PRINT_LOG_MESSAGE(log_object, ...)      \
    { \
        char log_buffer[256] = {0}; \
        snprintf(log_buffer, sizeof(log_buffer), __VA_ARGS__); \
        (log_object).log(log_buffer, (log_object).data); \
    }

// Used for test related dependency injection:
typedef int(*impl_clock)(struct timespec* now);
typedef const char*(*impl_get_system)(void);
typedef long(*impl_get_ncpus)(void);
typedef loadavgwatch_status(*impl_open)(const loadavgwatch_state* state, void** out_impl_state);
typedef loadavgwatch_status(*impl_close)(void* impl_state);
typedef loadavgwatch_status(*impl_get_load_average)(void* impl_state, float* out_loadavg);

typedef struct loadavgwatch_callbacks {
    impl_clock clock;
    impl_get_system get_system;
    impl_get_ncpus get_ncpus;
    impl_open open;
    impl_close close;
    impl_get_load_average get_load_average;
} loadavgwatch_callbacks;

struct _loadavgwatch_state
{
    float start_load;
    float stop_load;

    loadavgwatch_load start_load_fixed;
    loadavgwatch_load stop_load_fixed;

    struct timespec last_start_time;
    struct timespec last_stop_time;

    struct timespec last_over_start_load;
    struct timespec last_over_stop_load;
    struct timespec quiet_period_over_start;
    struct timespec quiet_period_over_stop;
    struct timespec start_interval;
    struct timespec stop_interval;

    loadavgwatch_log_object log_info;
    loadavgwatch_log_object log_error;
    loadavgwatch_log_object log_warning;

    loadavgwatch_callbacks impl;

    void* impl_state;
};

const char* loadavgwatch_impl_get_system(void);
long loadavgwatch_impl_get_ncpus(void);

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state);
loadavgwatch_status loadavgwatch_impl_close(void* impl_state);
loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* impl_state, float* out_loadavg);

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#endif // #ifndef LOADAVGWATCH_IMPL_H
