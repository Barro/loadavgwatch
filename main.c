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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct program_options
{
    char* start_load;
    char* stop_load;
    char* quiet_period_minutes;
    char* start_script;
    char* stop_script;
} program_options;

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

static unsigned g_child_execution_warning_timeout;
static const char* g_child_action;

static void
alarm_handler(int sig, siginfo_t* info, void* ucontext)
{
    static char warning_message[256];
    snprintf(
        warning_message,
        sizeof(warning_message),
        "Process for %s action took more that %u seconds to execute! "
        "You might want to see the README for hints for using this program.",
        g_child_action,
        g_child_execution_warning_timeout);
    log_warning(warning_message, stderr);
}

int main(int argc, char* argv[])
{
    {
        char version_info[128];
        snprintf(
            version_info,
            sizeof(version_info),
            "loadavgwatch 0.0.0 %s",
            get_system());
        log_info(version_info, stdout);
    }

    char start_load[sizeof("2147483648.00")] = "";
    char stop_load[sizeof("2147483648.00")] = "";
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

    loadavgwatch_log_object log_info_callback = {log_info, stdout};
    loadavgwatch_log_object log_warning_callback = {log_warning, stderr};
    loadavgwatch_log_object log_error_callback = {log_error, stderr};
    loadavgwatch_parameter init_parameters[] = {
        {"log-info", &log_info_callback},
        {"log-warning", &log_warning_callback},
        {"log-error", &log_error_callback},
        {"start-load", start_load},
        {"stop-load", stop_load},
        {NULL, NULL}
    };
    loadavgwatch_state* state;
    if (loadavgwatch_open(init_parameters, &state) != 0) {
        abort();
    }
    union {
        loadavgwatch_parameter array[4];
        struct {
            loadavgwatch_parameter quiet_period;
            loadavgwatch_parameter start_interval;
            loadavgwatch_parameter stop_interval;
            loadavgwatch_parameter _end;
        } s;
    } defaults = {
        .s.quiet_period = {"quiet-period", NULL},
        .s.start_interval = {"start-interval", NULL},
        .s.stop_interval = {"stop-interval", NULL},
        .s._end = {NULL, NULL}
    };
    loadavgwatch_parameters_get(state, defaults.array);
    printf("start-load: %s\n", start_load);
    printf("stop-load: %s\n", stop_load);
    printf("quiet-period: %0.2f min\n",
           ((struct timespec*)defaults.s.quiet_period.value)->tv_sec / 60.0);
    printf("start-interval: %0.2f min\n",
           ((struct timespec*)defaults.s.start_interval.value)->tv_sec / 60.0);
    printf("stop-interval: %0.2f min\n",
           ((struct timespec*)defaults.s.stop_interval.value)->tv_sec / 60.0);

    struct timespec sleep_time = {
        .tv_sec = 5,
        .tv_nsec = 0
    };

    struct sigaction alarm_action = {
        .sa_sigaction = alarm_handler,
    };
    sigaction(SIGALRM, &alarm_action, NULL);

    const char start_script[] = "sleep 15";
    const char stop_script[] = "sleep 15";
    while (true) {
        loadavgwatch_poll_result poll_result;
        if (loadavgwatch_poll(state, &poll_result) != 0) {
            abort();
        }
        printf("%u %u\n", poll_result.start_count, poll_result.stop_count);
        if (poll_result.start_count > 1) {
            g_child_action = "start";
            g_child_execution_warning_timeout = 10;
            alarm(10);
            int ret = system(start_script);
            alarm(0);
            if (ret != EXIT_SUCCESS) {
                log_warning("Child process exited with non-zero status.", stderr);
            }
        } else {
            g_child_action = "stop";
            g_child_execution_warning_timeout = 10;
            alarm(10);
            int ret = system(stop_script);
            alarm(0);
            if (ret != EXIT_SUCCESS) {
                log_warning("Child process exited with non-zero status.", stderr);
            }
        }
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
