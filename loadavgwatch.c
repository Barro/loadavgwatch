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

#include <assert.h>
#include "loadavgwatch.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRINT_LOG_MESSAGE(log_object, ...) \
    { \
        char log_buffer[256] = {0}; \
        snprintf(log_buffer, sizeof(log_buffer), __VA_ARGS__); \
        (log_object).log(log_buffer, (log_object).data); \
    }

typedef int(*clock_callback)(clockid_t clk_id, struct timespec* tp);

struct _loadavgwatch_state
{
    float last_load_average;
    float start_load;
    float stop_load;

    struct timespec last_poll;
    struct timespec last_start_time;
    struct timespec last_stop_time;

    struct timespec minimum_quiet_period;

    struct timespec start_interval;
    struct timespec stop_interval;

    clock_callback clock;

    loadavgwatch_log_object log_info;
    loadavgwatch_log_object log_error;
    loadavgwatch_log_object log_warning;

    FILE* loadavg_fp;
};

static void log_null(
    const char* message __attribute__((unused)),
    void* data __attribute__((unused)))
{
}

static void log_stderr(const char* message, void* data __attribute__((unused)))
{
    fprintf(stderr, "%s\n", message);
}

static loadavgwatch_status get_load_average(
    loadavgwatch_state* state, float* out_loadavg)
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

static loadavgwatch_status read_parameters(
    const loadavgwatch_init_parameter* parameters,
    loadavgwatch_state* inout_state)
{
    const loadavgwatch_init_parameter* current_parameter = parameters;
    const struct {
        const char* _first;
        const char* log_info;
        const char* log_warning;
        const char* log_error;
        const char* start_load;
        const char* stop_load;
        const char* _last;
    } valid_parameters = {
        .log_info = "log-info",
        .log_warning = "log-warning",
        .log_error = "log-error",
        .start_load = "start-load",
        .stop_load = "stop-load",
    };
    bool has_unknown = false;
    while (current_parameter->key != NULL) {
        if (strcmp(current_parameter->key, valid_parameters.log_info) == 0) {
            inout_state->log_info = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, valid_parameters.log_warning) == 0) {
            inout_state->log_warning = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, valid_parameters.log_error) == 0) {
            inout_state->log_error = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, valid_parameters.start_load) == 0) {
            char* load_value = (char*)current_parameter->value;
            char* endptr = NULL;
            float start_load = strtof(load_value, &endptr);
            if (endptr == current_parameter->value) {
                return LOADAVGWATCH_ERR_INVALID_PARAMETER;
            }
            inout_state->start_load = start_load;
        } else if (strcmp(current_parameter->key, valid_parameters.stop_load) == 0) {
            char* load_value = (char*)current_parameter->value;
            char* endptr = NULL;
            float stop_load = strtof(load_value, &endptr);
            if (endptr == current_parameter->value) {
                return LOADAVGWATCH_ERR_INVALID_PARAMETER;
            }
            inout_state->stop_load = stop_load;
        } else {
            // We haven't initialized the logger yet. Register the
            // problem, but don't do anything yet.
            has_unknown = true;
        }
        current_parameter++;
    }

    current_parameter = parameters;
    while (has_unknown && current_parameter->key != NULL) {
        bool is_valid = false;
        for (const char* const* parameter = &valid_parameters._first + 1;
             parameter < &valid_parameters._last;
             ++parameter) {
            if (strcmp(current_parameter->key, *parameter) == 0) {
                is_valid = true;
                break;
            }
        }
        if (!is_valid) {
            PRINT_LOG_MESSAGE(
                inout_state->log_warning,
                "Unknown parameter name: %s",
                current_parameter->key);
        }
        current_parameter++;
    }

    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_open(
    const loadavgwatch_init_parameter* parameters,
    loadavgwatch_state** out_state)
{
    *out_state = NULL;
    loadavgwatch_state* state = calloc(1, sizeof(loadavgwatch_state));
    if (state == NULL) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }

    // Set some defaults that can be overridden by either the using
    // application or test programs:
    state->clock = clock_gettime;
    state->log_info.log = log_null;
    state->log_error.log = log_stderr;
    state->log_warning.log = log_stderr;

    if (parameters != NULL) {
        int parameters_result = read_parameters(parameters, state);
        if (parameters_result != 0) {
            free(state);
            return parameters_result;
        }
    }

    FILE* loadavg_fp = fopen("/proc/loadavg", "r");
    if (loadavg_fp == NULL) {
        state->log_error.log(
            "Unable to open /proc/loadavg for reading!",
            state->log_error.data);
        free(state);
        return LOADAVGWATCH_ERR_INIT;
    }
    state->loadavg_fp = loadavg_fp;
    *out_state = state;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_close(loadavgwatch_state** state)
{
    if (*state == NULL) {
        return LOADAVGWATCH_OK;
    }
    fclose((*state)->loadavg_fp);
    free(*state);
    *state = NULL;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_poll(
    loadavgwatch_state* state, loadavgwatch_poll_result* out_result)
{
    assert(state != NULL && "Used uninitialized library! loadavgwatch_open() error codes!");
    float load_average;
    loadavgwatch_status read_status = get_load_average(state, &load_average);
    if (read_status != LOADAVGWATCH_OK) {
        return read_status;
    }
    PRINT_LOG_MESSAGE(state->log_info, "Load average: %0.2f", load_average);
    state->last_load_average = load_average;
    uint32_t start_count = 0;
    if (load_average < state->start_load) {
        start_count = (uint32_t)(state->start_load - load_average) + 1;
    }
    uint32_t stop_count = 0;
    if (load_average > state->stop_load) {
        stop_count = (uint32_t)(load_average - state->stop_load) + 1;
    }

    loadavgwatch_poll_result result = {
        .start_count = start_count,
        .stop_count = stop_count,
    };
    *out_result = result;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_register_start(loadavgwatch_state* state)
{
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_register_stop(loadavgwatch_state* state)
{
    return LOADAVGWATCH_OK;
}

