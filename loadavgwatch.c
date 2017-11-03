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
#include "loadavgwatch-impl.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_null(
    const char* message __attribute__((unused)),
    void* data __attribute__((unused)))
{
}

static void log_stderr(const char* message, void* data __attribute__((unused)))
{
    fprintf(stderr, "%s\n", message);
}

static const struct {
    const char* _first;
    const char* system;
    const char* log_info;
    const char* log_warning;
    const char* log_error;
    const char* start_load;
    const char* stop_load;
    const char* quiet_period;
    const char* start_interval;
    const char* stop_interval;
    const char* impl_clock;
    const char* impl_get_system;
    const char* impl_get_ncpus;
    const char* impl_open;
    const char* impl_close;
    const char* impl_get_load_average;
    const char* _last;
} VALID_PARAMETERS = {
    .system = "system",

    .log_info = "log-info",
    .log_warning = "log-warning",
    .log_error = "log-error",

    .start_load = "start-load",
    .stop_load = "stop-load",

    .quiet_period = "quiet-period",
    .start_interval = "start-interval",
    .stop_interval = "stop-interval",

    // Used for testing:
    .impl_get_system = "impl-get-system",
    .impl_get_ncpus = "impl-get-ncpus",
    .impl_clock = "impl-clock",
    .impl_open = "impl-open",
    .impl_close = "impl-close",
    .impl_get_load_average = "impl-get-load-average",
};

static void adjust_start_stop_loads(loadavgwatch_state* inout_state)
{
    // We have not initialized load values yet:
    if (strlen(inout_state->start_load_str) == 0
        || strlen(inout_state->stop_load_str) == 0) {
        return;
    }
    // Everything is OK, no adjustment needed.
    if (inout_state->start_load + 1.0 <= inout_state->stop_load) {
        return;
    }
    float new_start_load = inout_state->start_load - 1.0;
    PRINT_LOG_MESSAGE(
        inout_state->log_warning,
        "Start load (%s) must be at least one less than the stop "
        "load (%s). Forcing start load to be %0.2f.",
        inout_state->start_load_str,
        inout_state->stop_load_str,
        new_start_load);
    snprintf(
        inout_state->start_load_str, sizeof(inout_state->start_load_str),
        "%0.2f", new_start_load);
}


static bool check_for_unknown_parameters(
    loadavgwatch_state* state, const loadavgwatch_parameter* parameters)
{
    const loadavgwatch_parameter* current_parameter = parameters;
    bool all_valid = true;
    while (current_parameter->key != NULL) {
        bool current_valid = false;
        for (const char* const* parameter = &VALID_PARAMETERS._first + 1;
             parameter < &VALID_PARAMETERS._last;
             ++parameter) {
            if (strcmp(current_parameter->key, *parameter) == 0) {
                current_valid = true;
                break;
            }
        }
        if (!current_valid) {
            all_valid = false;
            PRINT_LOG_MESSAGE(
                state->log_warning,
                "Unknown parameter name: %s",
                current_parameter->key);
        }
        current_parameter++;
    }
    return all_valid;
}

static loadavgwatch_status read_parameters(
    loadavgwatch_state* inout_state, const loadavgwatch_parameter* parameters)
{
    const loadavgwatch_parameter* current_parameter = parameters;
    bool has_unknown = false;
    loadavgwatch_status return_status = LOADAVGWATCH_OK;
    const char* invalid_parameter_name = NULL;
    while (current_parameter->key != NULL) {
        if (strcmp(current_parameter->key, VALID_PARAMETERS.system) == 0) {
            // Make system parameter read-only:
            invalid_parameter_name = current_parameter->key;
            return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_info) == 0) {
            inout_state->log_info = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_warning) == 0) {
            inout_state->log_warning = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_error) == 0) {
            inout_state->log_error = *(
                (loadavgwatch_log_object*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_load) == 0) {
            char* load_value = (char*)current_parameter->value;
            if (strlen(load_value) > sizeof(inout_state->start_load_str) - 1) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            char* endptr = NULL;
            float start_load = strtof(load_value, &endptr);
            if (endptr == current_parameter->value) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            strcpy(inout_state->start_load_str, load_value);
            inout_state->start_load = start_load;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_load) == 0) {
            char* load_value = (char*)current_parameter->value;
            if (strlen(load_value) > sizeof(inout_state->stop_load_str) - 1) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            char* endptr = NULL;
            float stop_load = strtof(load_value, &endptr);
            if (endptr == current_parameter->value) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            if (stop_load < 1.0) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            strcpy(inout_state->stop_load_str, load_value);
            inout_state->stop_load = stop_load;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period) == 0) {
            inout_state->quiet_period = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_interval) == 0) {
            inout_state->start_interval = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_interval) == 0) {
            inout_state->stop_interval = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_clock) == 0) {
            inout_state->impl_clock = *((impl_clock*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_system) == 0) {
            // Well system is really not read-only. It's just overridable for unit tests...
            inout_state->impl_get_system = *((impl_get_system*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_ncpus) == 0) {
            inout_state->impl_get_ncpus = *((impl_get_ncpus*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_open) == 0) {
            inout_state->impl_open = *((impl_open*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_close) == 0) {
            inout_state->impl_close = *((impl_close*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_load_average) == 0) {
            inout_state->impl_get_load_average = *(
                (impl_get_load_average*)current_parameter->value);
        } else {
            // We haven't initialized the logger yet. Register the
            // problem, but don't do anything yet.
            has_unknown = true;
        }
        current_parameter++;
    }

    // Here we should have alternative loggers in use if standard
    // error logging has been overwritten:
    if (return_status == LOADAVGWATCH_ERR_INVALID_PARAMETER) {
        assert(invalid_parameter_name != NULL);
        PRINT_LOG_MESSAGE(
            inout_state->log_error,
            "Value for parameter %s was invalid!",
            invalid_parameter_name);
        return return_status;
    }

    if (has_unknown) {
        check_for_unknown_parameters(inout_state, parameters);
    }
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_open(
    loadavgwatch_parameter* parameters, loadavgwatch_state** out_state)
{
    *out_state = NULL;
    loadavgwatch_state* state = calloc(1, sizeof(loadavgwatch_state));
    if (state == NULL) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }

    // Set some defaults that can be overridden by either the using
    // application or test programs:
    state->log_info.log = log_null;
    state->log_error.log = log_stderr;
    state->log_warning.log = log_stderr;

    // Calling program will likely want to overwrite these as these
    // are really machine specific:

    // Default values for program starting/stopping related times:
    state->quiet_period = (struct timespec){
        .tv_sec = 60 * 60,
        .tv_nsec = 0
    };
    state->start_interval = (struct timespec){
        .tv_sec = 60 + 10,
        .tv_nsec = 0
    };
    state->stop_interval = (struct timespec){
        .tv_sec = 2 * 60,
        .tv_nsec = 0
    };

    // Defaults that mainly tests should be interested in overwriting:
    state->impl_clock = clock_gettime;
    state->impl_get_system = loadavgwatch_impl_get_system;
    state->impl_get_ncpus = loadavgwatch_impl_get_ncpus;
    state->impl_open = loadavgwatch_impl_open;
    state->impl_close = loadavgwatch_impl_close;
    state->impl_get_load_average = loadavgwatch_impl_get_load_average;

    if (parameters != NULL) {
        int parameters_result = read_parameters(state, parameters);
        if (parameters_result != 0) {
            free(state);
            return parameters_result;
        }
    }

    long ncpus = state->impl_get_ncpus();
    if (strlen(state->start_load_str) == 0) {
        if (ncpus > 0) {
            state->start_load= (float)(ncpus - 1) + 0.02;
        } else {
            state->start_load = 0.02;
            PRINT_LOG_MESSAGE(
                state->log_warning,
                "Could not detect the number of CPUs. "
                "Using the default start load value for 1 CPU! "
                "Please set load limits manually!");
        }
        snprintf(
            state->start_load_str, sizeof(state->start_load_str),
            "%0.2f", state->start_load);
    }

    if (strlen(state->stop_load_str) == 0) {
        if (ncpus > 0) {
            state->stop_load = (float)(ncpus) + 0.12;
        } else {
            state->stop_load = 1.12;
            PRINT_LOG_MESSAGE(
                state->log_warning,
                "Could not detect the number of CPUs. "
                "Using the default stop load value for 1 CPU! "
                "Please set load limits manually!");
        }
        snprintf(
            state->stop_load_str, sizeof(state->stop_load_str),
            "%0.2f", state->stop_load);
    }

    adjust_start_stop_loads(state);

    void* impl_state = NULL;
    loadavgwatch_status impl_open_result = state->impl_open(state, &impl_state);
    if (impl_open_result != LOADAVGWATCH_OK) {
        return impl_open_result;
    }
    state->impl_state = impl_state;

    *out_state = state;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_parameters_get(
    loadavgwatch_state* state, loadavgwatch_parameter* inout_parameters)
{
    if (!check_for_unknown_parameters(state, inout_parameters)) {
        return LOADAVGWATCH_ERR_INVALID_PARAMETER;
    }
    loadavgwatch_parameter* current_parameter = inout_parameters;
    while (current_parameter->key != NULL) {
        if (strcmp(current_parameter->key, VALID_PARAMETERS.system) == 0) {
            current_parameter->value = (char*)state->impl_get_system();
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_info) == 0) {
            current_parameter->value = &state->log_info;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_warning) == 0) {
            current_parameter->value = &state->log_warning;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.log_error) == 0) {
            current_parameter->value = &state->log_error;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_load) == 0) {
            current_parameter->value = state->start_load_str;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_load) == 0) {
            current_parameter->value = state->stop_load_str;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period) == 0) {
            current_parameter->value = &state->quiet_period;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_interval) == 0) {
            current_parameter->value = &state->start_interval;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_interval) == 0) {
            current_parameter->value = &state->stop_interval;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_clock) == 0) {
            current_parameter->value = state->impl_clock;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_system) == 0) {
            current_parameter->value = state->impl_get_system;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_ncpus) == 0) {
            current_parameter->value = state->impl_get_ncpus;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_open) == 0) {
            current_parameter->value = state->impl_open;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_close) == 0) {
            current_parameter->value = state->impl_close;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_get_load_average) == 0) {
            current_parameter->value = state->impl_get_load_average;
        } else {
            assert(false && "We should never reach here!");
        }
        current_parameter++;
    }
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_parameters_set(
    loadavgwatch_state* inout_state, loadavgwatch_parameter* parameters)
{
    loadavgwatch_status status = read_parameters(inout_state, parameters);
    adjust_start_stop_loads(inout_state);
    return status;
}

loadavgwatch_status loadavgwatch_close(loadavgwatch_state** state)
{
    if (*state == NULL) {
        return LOADAVGWATCH_OK;
    }
    (*state)->impl_close((*state)->impl_state);
    memset((*state), 0, sizeof(loadavgwatch_state));
    free(*state);
    *state = NULL;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_poll(
    loadavgwatch_state* state, loadavgwatch_poll_result* out_result)
{
    assert(state != NULL && "Used uninitialized library! loadavgwatch_open() error codes!");
    float load_average;
    loadavgwatch_status read_status = state->impl_get_load_average(
        state->impl_state, &load_average);
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

