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

#define _XOPEN_SOURCE 600

#include <assert.h>
#include "loadavgwatch.h"
#include "loadavgwatch-impl.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This corresponds to 1 month. Maximum sane intervals that the user
// if this library might probably want are in hours. Allow interval of
// several days for testing, but month is way too much. Except for
// timeouts, but this library does not handle them.
static const time_t MAX_INTERVAL_SECONDS = 2592000;

static void log_null(
    const char* message __attribute__((unused)),
    void* data __attribute__((unused)))
{
}

static void log_stderr(const char* message, void* data __attribute__((unused)))
{
    fprintf(stderr, "%s\n", message);
}

static struct {
    loadavgwatch_log_object warning;
    loadavgwatch_log_object error;
} LOG_DEFAULTS = {
    .warning = {log_stderr, NULL},
    .error = {log_stderr, NULL}
};

static const struct {
    const char* _first;
    const char* system;
    const char* log_info;
    const char* log_warning;
    const char* log_error;
    const char* start_load;
    const char* start_interval;
    const char* quiet_period_over_start;
    const char* stop_load;
    const char* stop_interval;
    const char* quiet_period_over_stop;
    const char* impl_callbacks;
    const char* _last;
} VALID_PARAMETERS = {
    .system = "system",

    .log_info = "log-info",
    .log_warning = "log-warning",
    .log_error = "log-error",

    .start_load = "start-load",
    .start_interval = "start-interval",
    .quiet_period_over_start = "quiet-period-over-start",

    .stop_load = "stop-load",
    .stop_interval = "stop-interval",
    .quiet_period_over_stop = "quiet-period-over-stop",

    // Used for testing:
    .impl_callbacks = "impl-callbacks",
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
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period_over_start) == 0) {
            if (((struct timespec*)current_parameter->value)->tv_sec > MAX_INTERVAL_SECONDS) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            inout_state->quiet_period_over_start = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period_over_stop) == 0) {
            if (((struct timespec*)current_parameter->value)->tv_sec > MAX_INTERVAL_SECONDS) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            inout_state->quiet_period_over_stop = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_interval) == 0) {
            if (((struct timespec*)current_parameter->value)->tv_sec > MAX_INTERVAL_SECONDS) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            inout_state->start_interval = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_interval) == 0) {
            if (((struct timespec*)current_parameter->value)->tv_sec > MAX_INTERVAL_SECONDS) {
                invalid_parameter_name = current_parameter->key;
                return_status = LOADAVGWATCH_ERR_INVALID_PARAMETER;
                continue;
            }
            inout_state->stop_interval = *(
                (struct timespec*)current_parameter->value);
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_callbacks) == 0) {
            loadavgwatch_callbacks* callbacks = current_parameter->value;
            // Callback overrides enable partial overrides by just setting parameters to NULL.
            if (callbacks->clock != NULL) {
                inout_state->impl.clock = callbacks->clock;
            }
            // Well system is really not read-only. It's just overridable for unit tests...
            if (callbacks->get_system != NULL) {
                inout_state->impl.get_system = callbacks->get_system;
            }
            if (callbacks->get_ncpus != NULL) {
                inout_state->impl.get_ncpus = callbacks->get_ncpus;
            }
            if (callbacks->open != NULL) {
                inout_state->impl.open = callbacks->open;
            }
            if (callbacks->close != NULL) {
                inout_state->impl.close = callbacks->close;
            }
            if (callbacks->get_load_average != NULL) {
                inout_state->impl.get_load_average = callbacks->get_load_average;
            }
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

/**
 * If there is a system that does not support clock_gettime(), make
 * this target implementation specific function.
 */
static int loadavgwatch_impl_clock(struct timespec* now)
{
    return clock_gettime(CLOCK_MONOTONIC, now);
}

loadavgwatch_status loadavgwatch_open_logging(
    loadavgwatch_state** out_state,
    loadavgwatch_log_object* log_warning,
    loadavgwatch_log_object* log_error)
{
    *out_state = NULL;
    loadavgwatch_state* state = calloc(1, sizeof(loadavgwatch_state));
    if (state == NULL) {
        return LOADAVGWATCH_ERR_OUT_OF_MEMORY;
    }

    // Set info log to be initially a null logger. We don't need info
    // logger in library initialization. At least for now:
    state->log_info.log = log_null;
    state->log_warning = *log_warning;
    state->log_error = *log_error;

    // Calling program will likely want to overwrite these as these
    // are really machine specific:

    // Default values for program starting/stopping related times:
    state->quiet_period_over_start = (struct timespec){
        .tv_sec = 15 * 60,
        .tv_nsec = 0
    };
    state->quiet_period_over_stop = (struct timespec){
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
    state->impl.clock = loadavgwatch_impl_clock;
    state->impl.get_system = loadavgwatch_impl_get_system;
    state->impl.get_ncpus = loadavgwatch_impl_get_ncpus;
    state->impl.open = loadavgwatch_impl_open;
    state->impl.close = loadavgwatch_impl_close;
    state->impl.get_load_average = loadavgwatch_impl_get_load_average;

    long ncpus = state->impl.get_ncpus();
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
    loadavgwatch_status impl_open_result = state->impl.open(state, &impl_state);
    if (impl_open_result != LOADAVGWATCH_OK) {
        return impl_open_result;
    }
    state->impl_state = impl_state;

    *out_state = state;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_open(loadavgwatch_state** out_state)
{
    return loadavgwatch_open_logging(
        out_state, &LOG_DEFAULTS.warning, &LOG_DEFAULTS.error);
}

loadavgwatch_status loadavgwatch_parameters_get(
    loadavgwatch_state* state, loadavgwatch_parameter* inout_parameters)
{
    assert(state != NULL && "Used uninitialized library!");
    if (!check_for_unknown_parameters(state, inout_parameters)) {
        return LOADAVGWATCH_ERR_INVALID_PARAMETER;
    }
    loadavgwatch_parameter* current_parameter = inout_parameters;
    while (current_parameter->key != NULL) {
        if (strcmp(current_parameter->key, VALID_PARAMETERS.system) == 0) {
            current_parameter->value = (char*)state->impl.get_system();
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
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period_over_start) == 0) {
            current_parameter->value = &state->quiet_period_over_start;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.quiet_period_over_stop) == 0) {
            current_parameter->value = &state->quiet_period_over_stop;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.start_interval) == 0) {
            current_parameter->value = &state->start_interval;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.stop_interval) == 0) {
            current_parameter->value = &state->stop_interval;
        } else if (strcmp(current_parameter->key, VALID_PARAMETERS.impl_callbacks) == 0) {
            current_parameter->value = &state->impl;
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
    assert(inout_state != NULL && "Used uninitialized library!");
    loadavgwatch_status status = read_parameters(inout_state, parameters);
    adjust_start_stop_loads(inout_state);
    return status;
}

loadavgwatch_status loadavgwatch_close(loadavgwatch_state** state)
{
    if (*state == NULL) {
        return LOADAVGWATCH_OK;
    }
    (*state)->impl.close((*state)->impl_state);
    memset((*state), 0, sizeof(loadavgwatch_state));
    free(*state);
    *state = NULL;
    return LOADAVGWATCH_OK;
}


static bool time_less_than(
    const struct timespec* left, const struct timespec* right)
{
    if (left->tv_sec < right->tv_sec) {
        return true;
    }
    if (right->tv_sec < left->tv_sec) {
        return false;
    }
    return left->tv_nsec < right->tv_nsec;
}

/**
 * Calculates the difference between two time values
 *
 * If the difference would be negative, this will then result in zero
 * time difference.
 */
static struct timespec time_difference(
    const struct timespec* bigger, const struct timespec* smaller)
{
    struct timespec result = { .tv_sec = 0, .tv_nsec = 0 };
    if (time_less_than(bigger, smaller)) {
        return result;
    }
    time_t diff_sec = bigger->tv_sec - smaller->tv_sec;
    long diff_nsec = bigger->tv_nsec - smaller->tv_nsec;
    if (diff_nsec < 0) {
        diff_sec--;
        diff_nsec += 1000000000;
    }
    result.tv_sec = diff_sec;
    result.tv_nsec = diff_nsec;
    return result;
}

loadavgwatch_status loadavgwatch_poll(
    loadavgwatch_state* state, loadavgwatch_poll_result* out_result)
{
    assert(state != NULL && "Used uninitialized library!");
    // Default no-change result in case the reader does not check the
    // status code of this command:
    loadavgwatch_poll_result result = {
        .start_count = 0,
        .stop_count = 0,
    };

    float load_average;
    loadavgwatch_status read_status = state->impl.get_load_average(
        state->impl_state, &load_average);
    if (read_status != LOADAVGWATCH_OK) {
        *out_result = result;
        return read_status;
    }
    struct timespec now;
    if (state->impl.clock(&now) != 0) {
        PRINT_LOG_MESSAGE(
            state->log_warning, "Unable to read current poll time!");
        *out_result = result;
        return LOADAVGWATCH_ERR_CLOCK;
    }

    if (load_average < state->start_load) {
        struct timespec start_difference = time_difference(
            &now, &state->last_start_time);
        bool start_not_too_often = time_less_than(
            &state->start_interval, &start_difference);
        struct timespec quiet_difference_start = time_difference(
            &now, &state->last_over_start_load);
        bool start_not_in_over_start_quiet_period = time_less_than(
            &state->quiet_period_over_start, &quiet_difference_start);
        struct timespec quiet_difference_stop = time_difference(
            &now, &state->last_over_stop_load);
        bool start_not_in_over_stop_quiet_period = time_less_than(
            &state->quiet_period_over_stop, &quiet_difference_stop);
        if (start_not_too_often
            && start_not_in_over_start_quiet_period
            && start_not_in_over_stop_quiet_period) {
            result.start_count = (uint32_t)(state->start_load - load_average) + 1;
        }
    } else {
        state->last_over_start_load = now;
    }

    if (load_average > state->stop_load) {
        struct timespec stop_difference = time_difference(
            &now, &state->last_stop_time);
        bool stop_not_too_often = time_less_than(
            &state->stop_interval, &stop_difference);
        if (stop_not_too_often) {
            result.stop_count = (uint32_t)(load_average - state->stop_load) + 1;
        }
        state->last_over_stop_load = now;
    }

    PRINT_LOG_MESSAGE(
        state->log_info,
        "Load average: %0.2f, start %u, stop %u.",
        load_average,
        result.start_count,
        result.stop_count);
    *out_result = result;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_register_start(loadavgwatch_state* state)
{
    assert(state != NULL && "Used uninitialized library!");
    if (state->impl.clock(&state->last_start_time) != 0) {
        PRINT_LOG_MESSAGE(
            state->log_warning, "Unable to register command start time!");
        return LOADAVGWATCH_ERR_CLOCK;
    }
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_register_stop(loadavgwatch_state* state)
{
    assert(state != NULL && "Used uninitialized library!");
    if (state->impl.clock(&state->last_stop_time) != 0) {
        PRINT_LOG_MESSAGE(
            state->log_warning, "Unable to register command stop time!");
        return LOADAVGWATCH_ERR_CLOCK;
    }
    return LOADAVGWATCH_OK;
}
