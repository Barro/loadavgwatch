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
    fwrite(message, strlen(message), 1, stderr);
    fwrite("\n", sizeof("\n") - 1, 1, stderr);
}

static struct {
    loadavgwatch_log_object warning;
    loadavgwatch_log_object error;
} LOG_DEFAULTS = {
    .warning = {log_stderr, NULL},
    .error = {log_stderr, NULL}
};

static void adjust_start_stop_loads(loadavgwatch_state* inout_state)
{
    // Everything is OK, no adjustment needed.
    if (inout_state->start_load + 1.0 <= inout_state->stop_load) {
        return;
    }
    assert(false && "TODO revamp this whole assumption!");
    float new_start_load = inout_state->start_load - 1.0;
    PRINT_LOG_MESSAGE(
        inout_state->log_warning,
        "Start load (%0.2f) must be at least one less than the stop "
        "load (%0.2f). Forcing start load to be %0.2f.",
        inout_state->start_load,
        inout_state->stop_load,
        new_start_load);
    inout_state->start_load = new_start_load;
    inout_state->start_load_fixed.scale = 256;
    inout_state->start_load_fixed.load = inout_state->start_load * inout_state->start_load_fixed.scale;
}

loadavgwatch_status loadavgwatch_set_log_info(
    loadavgwatch_state* state, loadavgwatch_log_object* log)
{
    state->log_info_obj = *log;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_set_log_warning(
    loadavgwatch_state* state, loadavgwatch_log_object* log)
{
    state->log_warning_obj = *log;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_set_log_error(
    loadavgwatch_state* state, loadavgwatch_log_object* log)
{
    state->log_error_obj = *log;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_set_start_load(
    loadavgwatch_state* state, const loadavgwatch_load* load)
{
    state->start_load = (double)load->load / load->scale;
    state->start_load_fixed = *load;
    return LOADAVGWATCH_OK;
}

const char* loadavgwatch_get_system(const loadavgwatch_state* state)
{
    return state->impl.get_system();
}

loadavgwatch_load loadavgwatch_get_start_load(const loadavgwatch_state* state)
{
    return state->start_load_fixed;
}

struct timespec loadavgwatch_get_start_interval(
    const loadavgwatch_state* state)
{
    return state->start_interval;
}

struct timespec loadavgwatch_get_quiet_period_over_start(
    const loadavgwatch_state* state)
{
    return state->quiet_period_over_start;
}

loadavgwatch_load loadavgwatch_get_stop_load(const loadavgwatch_state* state)
{
    return state->stop_load_fixed;
}

struct timespec loadavgwatch_get_stop_interval(
    const loadavgwatch_state* state)
{
    return state->stop_interval;
}

struct timespec loadavgwatch_get_quiet_period_over_stop(
    const loadavgwatch_state* state)
{
    return state->quiet_period_over_stop;
}

static loadavgwatch_status check_max_interval_set(
    loadavgwatch_state* state,
    const char* type,
    const struct timespec* interval,
    struct timespec* destination)
{
    if (interval->tv_sec > MAX_INTERVAL_SECONDS) {
        PRINT_LOG_MESSAGE(
            state->log_error,
            "Refusing to set %s of %lu seconds that is more than 1 month!",
            type,
            interval->tv_sec);
        return LOADAVGWATCH_ERR_INVALID_PARAMETER;
    }
    *destination = *interval;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_set_start_interval(
    loadavgwatch_state* state, const struct timespec* interval)
{
    return check_max_interval_set(
        state, "start interval", interval, &state->start_interval);
}

loadavgwatch_status loadavgwatch_set_quiet_period_over_start(
    loadavgwatch_state* state, const struct timespec* interval)
{
    return check_max_interval_set(
        state,
        "quiet period over start",
        interval,
        &state->quiet_period_over_start);
}

loadavgwatch_status loadavgwatch_set_stop_load(
    loadavgwatch_state* state, const loadavgwatch_load* load)
{
    state->stop_load = (double)load->load / load->scale;
    state->stop_load_fixed = *load;
    return LOADAVGWATCH_OK;
}

loadavgwatch_status loadavgwatch_set_stop_interval(
    loadavgwatch_state* state, const struct timespec* interval)
{
    return check_max_interval_set(
        state, "stop interval", interval, &state->stop_interval);
}

loadavgwatch_status loadavgwatch_set_quiet_period_over_stop(
    loadavgwatch_state* state, const struct timespec* interval)
{
    return check_max_interval_set(
        state,
        "quiet period over stop",
        interval,
        &state->quiet_period_over_stop);
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
    state->log_info_obj.log = log_null;
    state->log_info = &state->log_info_obj;
    state->log_warning_obj = *log_warning;
    state->log_warning = &state->log_warning_obj;
    state->log_error_obj = *log_error;
    state->log_error = &state->log_error_obj;

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
    if (ncpus > 0) {
        state->start_load = (float)(ncpus - 1) + 0.02;
    } else {
        state->start_load = 0.02;
        PRINT_LOG_MESSAGE(
            state->log_warning,
            "Could not detect the number of CPUs. "
            "Using the default start load value for 1 CPU! "
            "Please set load limits manually!");
    }
    state->start_load_fixed.scale = 256;
    state->start_load_fixed.load = state->start_load * state->start_load_fixed.scale;

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
    state->stop_load_fixed.scale = 256;
    state->stop_load_fixed.load = state->stop_load * state->stop_load_fixed.scale;

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
    adjust_start_stop_loads(state);
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
        PRINT_LOG_MESSAGE(
            state->log_warning, "Unable to read the current load average!");
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
