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

#include "loadavgwatch.h"
#include "main-impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

static void log_info(const char* message, void* stream)
{
    fprintf((FILE*)stream, "%s\n", message);
}

static void log_warning(const char* message, void* stream)
{
    fprintf((FILE*)stream, "warning: %s\n", message);
}

static void log_error(const char* message, void* stream)
{
    fprintf((FILE*)stream, "ERROR: %s\n", message);
}

int main(int argc, char* argv[])
{
    char start_load[sizeof("2147483648.00")] = "0.02";
    char stop_load[sizeof("2147483648.00")] = "1.12";
    loadavgwatch_log_object log_info_callback = {log_info, stdout};
    loadavgwatch_log_object log_warning_callback = {log_warning, stderr};
    loadavgwatch_log_object log_error_callback = {log_error, stderr};
    loadavgwatch_init_parameter parameters[] = {
        {"start-load", start_load},
        {"stop-load", stop_load},
        {"log-info", &log_info_callback},
        {"log-warning", &log_warning_callback},
        {"log-error", &log_error_callback},
        {NULL, NULL}
    };

    long ncpus = get_ncpus();
    if (ncpus > 0) {
        snprintf(start_load, sizeof(start_load), "%0.2f", (float)(ncpus - 1) + 0.02);
        snprintf(stop_load, sizeof(stop_load), "%0.2f", (float)(ncpus) + 0.12);
    } else {
        log_warning(
            "Could not detect the number of CPUs. "
            "Using the default load values for 1 CPU! "
            "Please set load limits manually!",
            stderr);
    }
    loadavgwatch_state* state;
    if (loadavgwatch_open(parameters, &state) != 0) {
        abort();
    }

    struct timespec sleep_time = {
        .tv_sec = 5,
        .tv_nsec = 0
    };

    while (true) {
        loadavgwatch_poll_result poll_result;
        if (loadavgwatch_poll(state, &poll_result) != 0) {
            abort();
        }
        printf("%u %u\n", poll_result.start_count, poll_result.stop_count);
        int sleep_return = -1;
        struct timespec sleep_remaining = sleep_time;
        do {
            sleep_return = nanosleep(&sleep_remaining, &sleep_remaining);
        } while (sleep_return == -1);
    }

    if (loadavgwatch_close(&state) != 0) {
        abort();
    }
    return EXIT_SUCCESS;
}
