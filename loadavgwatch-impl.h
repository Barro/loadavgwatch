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
typedef int(*impl_clock)(clockid_t clk_id, struct timespec* tp);
typedef loadavgwatch_status(*impl_open)(const loadavgwatch_state* state, void** out_impl_state);
typedef loadavgwatch_status(*impl_close)(void* impl_state);
typedef loadavgwatch_status(*impl_get_load_average)(void* impl_state, float* out_loadavg);

struct _loadavgwatch_state
{
    float last_load_average;
    float start_load;
    float stop_load;

    struct timespec last_poll;
    struct timespec last_start_time;
    struct timespec last_stop_time;

    struct timespec quiet_period;
    struct timespec start_interval;
    struct timespec stop_interval;

    loadavgwatch_log_object log_info;
    loadavgwatch_log_object log_error;
    loadavgwatch_log_object log_warning;

    impl_clock impl_clock;
    impl_open impl_open;
    impl_close impl_close;
    impl_get_load_average impl_get_load_average;

    void* impl_state;
};

loadavgwatch_status loadavgwatch_impl_open(
    const loadavgwatch_state* state, void** out_impl_state);
loadavgwatch_status loadavgwatch_impl_close(void* impl_state);
loadavgwatch_status loadavgwatch_impl_get_load_average(
    void* impl_state, float* out_loadavg);

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#endif // #ifndef LOADAVGWATCH_IMPL_H
