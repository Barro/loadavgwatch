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

#include <loadavgwatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct program_options
{
    // Defaults given by the library:
    const char* start_load;
    const char* start_interval;
    const char* quiet_period_over_start;
    const char* stop_load;
    const char* stop_interval;
    const char* quiet_period_over_stop;

    // These values are used inside main() to do actions:
    const char* start_script;
    const char* stop_script;
    bool dry_run;
    bool has_timeout;
    struct timespec timeout;
} program_options;

typedef enum setup_options_result {
    OPTIONS_OK = 0,
    OPTIONS_HELP = 1,
    OPTIONS_VERSION = 2,
    OPTIONS_FAILURE = -1
} setup_options_result;

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

static int init_library(loadavgwatch_state** out_state)
{
    loadavgwatch_log_object log_info_callback = {log_info, stdout};
    loadavgwatch_log_object log_warning_callback = {log_warning, stderr};
    loadavgwatch_log_object log_error_callback = {log_error, stderr};
    loadavgwatch_parameter init_parameters[] = {
        {"log-info", &log_info_callback},
        {"log-warning", &log_warning_callback},
        {"log-error", &log_error_callback},
        {NULL, NULL}
    };
    loadavgwatch_status open_ret = loadavgwatch_open(init_parameters, out_state);
    switch (open_ret) {
    case LOADAVGWATCH_ERR_OUT_OF_MEMORY:
        log_error("Out of memory in library initialization!", stderr);
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_READ:
        log_error(
            "Read error in library initialization! Check file access rights!",
            stderr);
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_INIT:
    case LOADAVGWATCH_ERR_PARSE:
    case LOADAVGWATCH_ERR_CLOCK:
        log_error("Unknown library initialization error!", stderr);
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_INVALID_PARAMETER:
        log_warning(
            "Invalid library parameter! Is this program linked correctly?",
            stderr);
        break;
    case LOADAVGWATCH_OK:
        break;
    default:
        abort();
    }
    return EXIT_SUCCESS;
}

static void show_version(const program_options* program_options)
{
    printf("loadavgwatch %s %s\n", "0.0.0", "linux");
    printf("Copyright (C) 2017 Jussi Judin\n");
    printf("License GPLv3: GNU GPL version 3 <https://gnu.org/licenses/gpl.html>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static void show_help(const program_options* program_options, char* argv[])
{
    printf("Usage: %s [options]\n", argv[0]);
    printf(
"Execute actions based on the current machine load (1 minute load average).\n"
"\n"
"Options:\n"
"  -h, --help           Show this help.\n"
"  --start-load <value> Maximum load value where we still execute the start command (%s).\n"
"  --stop-load <value>  Minimum load value where we start executing the stop command (%s).\n"
"  -s, --start-command <command>\n"
"                       Command to run while we still are under the start load value.\n"
"  -t, --stop-command <command>\n"
"                       Command to run when we go over the stop load limit.\n"
"  --start-interval <time>\n"
"                       Time we wait between subsequent start command runs (%s).\n"
"  --stop-interval <time>\n"
"                       Time we wait between subsequent start command runs (%s).\n"
"  --timeout <time>     Execute only for specified amount of time. Otherwise run until interrupted.\n"
"  --dry-run            Do not run any commands. Only show what would be done.\n"
"  --version            Show version information.\n",
"default-start-load",
"default-stop-load",
"default-start-interval",
"default-stop-interval"
        );
}

static setup_options_result setup_options(
    loadavgwatch_state* state,
    int argc,
    char* argv[],
    program_options* out_program_options)
{
    union {
        struct _defaults {
            loadavgwatch_parameter system;
            loadavgwatch_parameter start_load;
            loadavgwatch_parameter start_interval;
            loadavgwatch_parameter quiet_period_over_start;
            loadavgwatch_parameter stop_load;
            loadavgwatch_parameter stop_interval;
            loadavgwatch_parameter quiet_period_over_stop;
            loadavgwatch_parameter _end;
        } s;
        loadavgwatch_parameter array[
            sizeof(struct _defaults) / sizeof(((struct _defaults*)0)->_end)];
    } defaults = {
        .s.system = {"system", NULL},
        .s.start_load = {"start-load", NULL},
        .s.start_interval = {"start-interval", NULL},
        .s.quiet_period_over_start = {"quiet-period-over-start", NULL},
        .s.stop_load = {"stop-load", NULL},
        .s.stop_interval = {"stop-interval", NULL},
        .s.quiet_period_over_stop = {"quiet-period-over-stop", NULL},
        .s._end = {NULL, NULL}
    };
    if (loadavgwatch_parameters_get(state, defaults.array) != LOADAVGWATCH_OK) {
        log_error("Failed to get the default parameter values", stderr);
        return OPTIONS_FAILURE;
    }

    /* struct */
    /* { */
    /*     char* show_help; */
    /*     char* dry_run; */
    /*     char* start_load; */
    /*     char* stop_load; */
    /*     char* quiet_period_minutes; */
    /*     char* start_script; */
    /*     char* stop_script; */
    /* } command_line_args = { NULL }; */

    for (int argument = 1; argument < argc; ++argument) {
        const char* current_argument = argv[argument];
        if (strcmp(current_argument, "--help") == 0 || strcmp(current_argument, "-h") == 0) {
            show_help(out_program_options, argv);
            return OPTIONS_HELP;
        }
        if (strcmp(current_argument, "--version") == 0) {
            show_version(out_program_options);
            return OPTIONS_VERSION;
        }
        if (strcmp(current_argument, "--timeout") == 0) {
            int next_index = argument + 1;
            if (next_index >= argc) {
                log_error("No value for --timeout option!", stderr);
                return OPTIONS_FAILURE;
            }
            ++argument;
            current_argument = argv[argument];

            if (strcmp(current_argument, "0") != 0) {
                log_error("TODO non-zero timeout", stderr);
                return OPTIONS_FAILURE;
            }
            out_program_options->has_timeout = true;
            out_program_options->timeout.tv_sec = 0;
            out_program_options->timeout.tv_nsec = 0;
        }
    }

    printf("start-load: %s\n", (const char*)defaults.s.start_load.value);
    printf("stop-load: %s\n", (const char*)defaults.s.stop_load.value);
    printf("quiet-period-over-start: %0.2f min\n",
           ((const struct timespec*)defaults.s.quiet_period_over_start.value)->tv_sec / 60.0);
    printf("quiet-period-over-stop: %0.2f min\n",
           ((const struct timespec*)defaults.s.quiet_period_over_stop.value)->tv_sec / 60.0);
    printf("start-interval: %0.2f min\n",
           ((const struct timespec*)defaults.s.start_interval.value)->tv_sec / 60.0);
    printf("stop-interval: %0.2f min\n",
           ((const struct timespec*)defaults.s.stop_interval.value)->tv_sec / 60.0);
    return OPTIONS_OK;
}

static int monitor_and_act(
    loadavgwatch_state* state, program_options* options)
{
    // 3 pollings in 1 minute should result in high enough default
    // polling rate to catch relatively soon 1 minute load average
    // changes.
    struct timespec sleep_time = {
        .tv_sec = 20,
        .tv_nsec = 0
    };
    // If start or stop interval is smaller than the default sleep
    // time, adjust it down.
    // TODO...

    struct sigaction alarm_action = {
        .sa_sigaction = alarm_handler,
    };
    sigaction(SIGALRM, &alarm_action, NULL);

    const char start_script[] = "date; echo start";
    const char stop_script[] = "date; echo stop";
    bool running = true;
    // TODO override timeouts:
    bool has_timeout = true;
    struct timespec timeout = { .tv_sec = 30, .tv_nsec = 0 };
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        log_error("Unable to register program start time!", stderr);
        return EXIT_FAILURE;
    }
    struct timespec end_time = {
        .tv_sec = start_time.tv_sec + timeout.tv_sec };
    while (running) {
        loadavgwatch_poll_result poll_result;
        if (loadavgwatch_poll(state, &poll_result) != LOADAVGWATCH_OK) {
            abort();
        }
        if (poll_result.start_count > 0) {
            loadavgwatch_register_start(state);
            g_child_action = "start";
            g_child_execution_warning_timeout = 10;
            alarm(10);
            int ret = system(start_script);
            alarm(0);
            if (ret != EXIT_SUCCESS) {
                log_warning("Child process exited with non-zero status.", stderr);
            }
        }
        if (poll_result.stop_count > 0) {
            loadavgwatch_register_stop(state);
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
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            log_error("Unable to register current time!", stderr);
            return EXIT_FAILURE;
        }
        struct timespec sleep_remaining = sleep_time;
        do {
            sleep_return = nanosleep(&sleep_remaining, &sleep_remaining);
        } while (sleep_return == -1);
        if (has_timeout && now.tv_sec < end_time.tv_sec) {
            log_info("Timeout reached.", stdout);
            running = false;
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    loadavgwatch_state* state;
    {
        int result = init_library(&state);
        if (result != EXIT_SUCCESS) {
            return result;
        }
    }
    program_options program_options;
    {
        setup_options_result result = setup_options(
            state, argc, argv, &program_options);
        if (result == OPTIONS_FAILURE) {
            return EXIT_FAILURE;
        } else if (result != OPTIONS_OK) {
            return EXIT_SUCCESS;
        }
    }
    int program_result = monitor_and_act(state, &program_options);

    if (loadavgwatch_close(&state) != LOADAVGWATCH_OK) {
        log_error(
            "Unable to close the library! This should never happen", stderr);
        program_result = EXIT_FAILURE;
    }
    return program_result;
}
