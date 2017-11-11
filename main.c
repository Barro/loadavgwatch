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
#include "main-parsers.c"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PRINT_LOG_MESSAGE(log_object, ...)      \
    { \
        char log_buffer[256] = {0}; \
        snprintf(log_buffer, sizeof(log_buffer), __VA_ARGS__); \
        (log_object).log(log_buffer, (log_object).data); \
    }

typedef struct program_options
{
    // Defaults given by the library:
    const char* start_load;
    const struct timespec* start_interval;
    const struct timespec* quiet_period_over_start;
    const char* stop_load;
    const struct timespec* stop_interval;
    const struct timespec* quiet_period_over_stop;

    // These values are used inside main() to do actions:
    const char* start_command;
    const char* stop_command;
    bool has_timeout;
    struct timespec timeout;
    bool dry_run;
    bool verbose;
} program_options;

typedef enum setup_options_result {
    OPTIONS_OK = 0,
    OPTIONS_HELP = 1,
    OPTIONS_VERSION = 2,
    OPTIONS_FAILURE = -1
} setup_options_result;

static void log_null(const char* message, void* stream)
{
}

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

static struct {
    loadavgwatch_log_object info;
    loadavgwatch_log_object warning;
    loadavgwatch_log_object error;
} g_log;

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
    loadavgwatch_status open_ret = loadavgwatch_open_logging(
        out_state, &g_log.warning, &g_log.error);
    switch (open_ret) {
    case LOADAVGWATCH_ERR_OUT_OF_MEMORY:
        PRINT_LOG_MESSAGE(
            g_log.error, "Out of memory in library initialization!");
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_READ:
        PRINT_LOG_MESSAGE(
            g_log.error,
            "Read error in library initialization! Check file access rights!");
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_INIT:
    case LOADAVGWATCH_ERR_PARSE:
    case LOADAVGWATCH_ERR_CLOCK:
        PRINT_LOG_MESSAGE(
            g_log.error, "Unknown library initialization error!");
        return EXIT_FAILURE;
    case LOADAVGWATCH_ERR_INVALID_PARAMETER:
        PRINT_LOG_MESSAGE(
            g_log.warning,
            "Invalid library parameter! Is this program linked correctly?");
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

#define PROGRAM_OPTION_TIMESPEC_TO_STRING(option_name) \
    char option_name[32] = ""; \
    _timespec_to_string( \
        program_options->option_name, option_name, sizeof(option_name))

static void show_help(const program_options* program_options, char* argv[])
{
    PROGRAM_OPTION_TIMESPEC_TO_STRING(quiet_period_over_start);
    PROGRAM_OPTION_TIMESPEC_TO_STRING(quiet_period_over_stop);
    PROGRAM_OPTION_TIMESPEC_TO_STRING(start_interval);
    PROGRAM_OPTION_TIMESPEC_TO_STRING(stop_interval);
    printf("Usage: %s [options]\n", argv[0]);
    printf(
"Execute actions based on the current machine load (1 minute load average).\n"
"\n"
"Options:\n"
"  -h, --help           Show this help.\n"
"  -s, --start-command <command>\n"
"                       Command to run while we still are under the start load value.\n"
"  -t, --stop-command <command>\n"
"                       Command to run when we go over the stop load limit.\n"
);
printf(
"  --start-load <value> Maximum load value where we still execute the start command (%s).\n"
"  --stop-load <value>  Minimum load value where we start executing the stop command (%s).\n",
program_options->start_load,
program_options->stop_load
);
printf(
"  --quiet-period-over-start <time>\n"
"                       Do not start new processes for this long (%s) when start load (%s) has been exceeded.\n"
"  --quiet-period-over-stop <time>\n"
"                       Do not start new processes for this long (%s) when stop load (%s) has been exceeded.\n",
quiet_period_over_start,
program_options->start_load,
quiet_period_over_stop,
program_options->stop_load
);
printf(
"  --start-interval <time>\n"
"                       Time we wait between subsequent start commands (%s).\n"
"  --stop-interval <time>\n"
"                       Time we wait between subsequent stop commands (%s).\n",
start_interval,
stop_interval
);
printf(
"  --timeout <time>     Execute only for specified amount of time. Otherwise run until interrupted.\n"
"  --dry-run            Do not run any commands. Only show what would be done.\n"
"  -v, --verbose        Show verbose output.\n"
"  --version            Show version information.\n"
);
}

/* static bool parse_option_argument( */
/*     int argc, */
/*     char* argv[], */
/*     int current_index, */
/*     char* out_value, */
/*     int* next_index) */
/* { */
    
/* } */

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

    out_program_options->start_load = (const char*)defaults.s.start_load.value;
    out_program_options->start_interval = (const struct timespec*)
        defaults.s.start_interval.value;
    out_program_options->quiet_period_over_start =
        (const struct timespec*)defaults.s.quiet_period_over_start.value;
    out_program_options->stop_load = (const char*)defaults.s.stop_load.value;
    out_program_options->stop_interval = (const struct timespec*)
        defaults.s.stop_interval.value;
    out_program_options->quiet_period_over_stop =
        (const struct timespec*)defaults.s.quiet_period_over_stop.value;

    // Default values:
    out_program_options->start_command = NULL;
    out_program_options->stop_command = NULL;
    out_program_options->has_timeout = false;
    out_program_options->dry_run = false;
    out_program_options->verbose = false;

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
        char* current_argument = argv[argument];
        if (strcmp(current_argument, "--help") == 0 || strcmp(current_argument, "-h") == 0) {
            show_help(out_program_options, argv);
            return OPTIONS_HELP;
        } else if (strcmp(current_argument, "--start-command") == 0
                   || strcmp(current_argument, "-s") == 0) {
            if (argument + 1 >= argc) {
                log_error("No value for --start-command option!", stderr);
                return OPTIONS_FAILURE;
            }
            if (out_program_options->start_command != NULL) {
                log_error(
                    "Option --start-command has already been specified!",
                    stderr);
                return OPTIONS_FAILURE;
            }

            ++argument;
            current_argument = argv[argument];
            out_program_options->start_command = current_argument;
        } else if (strcmp(current_argument, "--stop-command") == 0
                   || strcmp(current_argument, "-t") == 0) {
            if (argument + 1 >= argc) {
                log_error("No value for --stop-command option!", stderr);
                return OPTIONS_FAILURE;
            }
            if (out_program_options->stop_command != NULL) {
                log_error(
                    "Option --stop-command has already been specified!",
                    stderr);
                return OPTIONS_FAILURE;
            }

            ++argument;
            current_argument = argv[argument];
            out_program_options->stop_command = current_argument;
        } else if (strcmp(current_argument, "--version") == 0) {
            show_version(out_program_options);
            return OPTIONS_VERSION;
        } else if (strcmp(current_argument, "--timeout") == 0) {
            int next_index = argument + 1;
            if (next_index >= argc) {
                log_error("No value for --timeout option!", stderr);
                return OPTIONS_FAILURE;
            }
            ++argument;
            current_argument = argv[argument];

            struct timespec timeout;
            if (!_string_to_timespec(current_argument, &timeout)) {
                log_error("Not a valid --timeout value!", stderr);
                return OPTIONS_FAILURE;
            }
            out_program_options->has_timeout = true;
            out_program_options->timeout = timeout;
        } else if (strcmp(current_argument, "--verbose") == 0
                   || strcmp(current_argument, "-v") == 0) {
            g_log.info.log = log_info;
            loadavgwatch_parameter verbose_param[] = {
                {"log-info", &g_log.info},
                {NULL, NULL}
            };
            loadavgwatch_parameters_set(state, verbose_param);
            out_program_options->verbose = true;
        }
    }

    return OPTIONS_OK;
}

static void run_command(
    const char* command, const char* child_action)
{
    PRINT_LOG_MESSAGE(g_log.info, "Running %s", command);
    g_child_action = child_action;
    g_child_execution_warning_timeout = 10;
    alarm(10);
    int ret = system(command);
    alarm(0);
    if (ret != EXIT_SUCCESS) {
        PRINT_LOG_MESSAGE(
            g_log.warning,
            "Child process exited with non-zero status %d.",
            ret);
    }
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

    bool running = true;
    // TODO override timeouts:
    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        log_error("Unable to register program start time!", stderr);
        return EXIT_FAILURE;
    }
    struct timespec end_time = {
        .tv_sec = start_time.tv_sec + options->timeout.tv_sec };
    while (running) {
        loadavgwatch_poll_result poll_result;
        if (loadavgwatch_poll(state, &poll_result) != LOADAVGWATCH_OK) {
            abort();
        }
        if (poll_result.start_count > 0) {
            loadavgwatch_register_start(state);
            if (options->start_command != NULL) {
                run_command(options->start_command, "start");
            }
        }
        if (poll_result.stop_count > 0) {
            loadavgwatch_register_stop(state);
            if (options->stop_command != NULL) {
                run_command(options->stop_command, "stop");
            }
        }
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            PRINT_LOG_MESSAGE(
                g_log.error, "Unable to register the current time!");
            return EXIT_FAILURE;
        }
        struct timespec sleep_remaining = sleep_time;
        if (options->has_timeout
            && end_time.tv_sec < now.tv_sec + sleep_time.tv_sec) {
            PRINT_LOG_MESSAGE(g_log.info, "Timeout reached.");
            running = false;
            if (end_time.tv_sec <= now.tv_sec) {
                sleep_remaining.tv_sec = 0;
            } else {
                sleep_remaining.tv_sec = now.tv_sec - end_time.tv_sec;
            }
        }
        int sleep_return = -1;
        do {
            sleep_return = nanosleep(&sleep_remaining, &sleep_remaining);
        } while (sleep_return == -1);
    }
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    g_log.info.log = log_null;
    g_log.info.data = stdout;
    g_log.warning.log = log_warning;
    g_log.warning.data = stderr;
    g_log.error.log = log_error;
    g_log.error.data = stderr;

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
