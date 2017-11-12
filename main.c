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
#include <loadavgwatch.h>
#include "main-parsers.c"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PRINTF_LOG_MESSAGE(log_object, ...)      \
    { \
        char log_buffer[256] = {0}; \
        snprintf(log_buffer, sizeof(log_buffer), __VA_ARGS__); \
        (log_object).log(log_buffer, (log_object).data); \
    }

#define PRINT_LOG_MESSAGE(log_object, message)      \
    { \
        (log_object).log(message, (log_object).data); \
    }

typedef struct program_options
{
    // These also include defaults given by the library:
    const char* arg_start_load;
    loadavgwatch_load start_load;
    const char* arg_start_interval;
    struct timespec start_interval;
    const char* arg_quiet_period_over_start;
    struct timespec quiet_period_over_start;
    const char* arg_stop_load;
    loadavgwatch_load stop_load;
    const char* arg_stop_interval;
    struct timespec stop_interval;
    const char* arg_quiet_period_over_stop;
    struct timespec quiet_period_over_stop;

    // These values are used inside main() to do actions:
    const char* start_command;
    const char* stop_command;
    bool has_timeout;
    const char* arg_timeout;
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

typedef struct option_argument {
    const char* name;
    const char** previous_value;
} option_argument;

typedef struct timespec_argument {
    const char* name;
    const char* value_str;
    struct timespec* destination;
} timespec_argument;

static void write_time_prefix(FILE* stream)
{
    const char* format = "%H:%M:%S%z";
    struct timespec now;
    assert(clock_gettime(CLOCK_REALTIME, &now) == 0);
    struct tm current_time;
    struct tm* current_time_p = localtime_r(&now.tv_sec, &current_time);
    assert(current_time_p != NULL);
    char date[] = "00:00:00+0000";
    size_t printed = strftime(date, sizeof(date), format, current_time_p);
    assert(printed > 0);
    fwrite(date, printed, 1, stream);
    fwrite(" ", 1, 1, stream);
}

static void log_null(const char* message, void* stream)
{
}

static void log_message(const char* message, void* stream)
{
    FILE* write_stream = (FILE*)stream;
    write_time_prefix(write_stream);
    fprintf(write_stream, "%s\n", message);
}

static void log_warning(const char* message, void* stream)
{
    FILE* write_stream = (FILE*)stream;
    write_time_prefix(write_stream);
    fprintf(write_stream, "warning: %s\n", message);
}

static void log_error(const char* message, void* stream)
{
    FILE* write_stream = (FILE*)stream;
    write_time_prefix(write_stream);
    fprintf(write_stream, "ERROR: %s\n", message);
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
        &program_options->option_name, option_name, sizeof(option_name))

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
    float start_load = (double)program_options->start_load.load / program_options->start_load.scale;
    float stop_load = (double)program_options->stop_load.load / program_options->stop_load.scale;
    printf(
"  --start-load <value> Maximum load value where we still execute the start command (%0.2f).\n"
"  --stop-load <value>  Minimum load value where we start executing the stop command (%0.2f).\n",
start_load,
stop_load
);
printf(
"  --quiet-period-over-start <time>\n"
"                       Do not start new processes for this long (%s) when start load (%0.2f) has been exceeded.\n"
"  --quiet-period-over-stop <time>\n"
"                       Do not start new processes for this long (%s) when stop load (%0.2f) has been exceeded.\n",
quiet_period_over_start,
start_load,
quiet_period_over_stop,
stop_load
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

static bool parse_option_argument(
    const char* option_name,
    const int argc,
    char* argv[],
    const int option_index,
    const char** inout_value,
    int* out_index)
{
    if (*inout_value != NULL) {
        PRINTF_LOG_MESSAGE(
            g_log.error,
            "Option %s has already been specified with value '%s'",
            option_name,
            *inout_value);
        return false;
    }
    char* equal_sign = strchr(argv[option_index], '=');
    if (equal_sign != NULL) {
        *inout_value = equal_sign + 1;
        *out_index = option_index;
        return true;
    }
        if (strcmp(option_name, argv[option_index]) != 0) {
            return false;
        }
    int value_index = option_index + 1;
    if (argc <= value_index) {
        PRINTF_LOG_MESSAGE(
            g_log.error,
            "No value given for %s option!",
            argv[option_index]);
        return false;
    }
    *inout_value = argv[value_index];
    *out_index = value_index;
    return true;
}

static bool argument_name_matches(
    const char* wanted_name,
    const char* current_argument)
{
    char* equal_sign = strchr(current_argument, '=');
    if (equal_sign == NULL) {
        return strcmp(wanted_name, current_argument) == 0;
    }
    if (strstr(current_argument, wanted_name) != current_argument) {
        return false;
    }
    return strncmp(
        wanted_name, current_argument, equal_sign - current_argument) == 0;
}

static setup_options_result setup_options(
    loadavgwatch_state* state,
    int argc,
    char* argv[],
    program_options* out_program_options)
{
    out_program_options->arg_start_load = NULL;
    out_program_options->start_load = loadavgwatch_get_start_load(state);
    out_program_options->arg_start_interval = NULL;
    out_program_options->start_interval = loadavgwatch_get_start_interval(state);
    out_program_options->arg_quiet_period_over_start = NULL;
    out_program_options->quiet_period_over_start = loadavgwatch_get_quiet_period_over_start(state);
    out_program_options->arg_stop_load = NULL;
    out_program_options->stop_load = loadavgwatch_get_stop_load(state);
    out_program_options->arg_stop_interval = NULL;
    out_program_options->stop_interval = loadavgwatch_get_stop_interval(state);
    out_program_options->arg_quiet_period_over_stop = NULL;
    out_program_options->quiet_period_over_stop = loadavgwatch_get_quiet_period_over_stop(state);

    // Default values:
    out_program_options->start_command = NULL;
    out_program_options->stop_command = NULL;
    out_program_options->has_timeout = false;
    out_program_options->arg_timeout = NULL;
    out_program_options->dry_run = false;
    out_program_options->verbose = false;

    option_argument option_arguments[] = {
        {"--start-command", &out_program_options->start_command},
        {"-s", &out_program_options->start_command},
        {"--stop-command", &out_program_options->stop_command},
        {"-t", &out_program_options->stop_command},
        {"--start-load", &out_program_options->arg_start_load},
        {"--start-interval", &out_program_options->arg_start_interval},
        {"--quiet-period-over-start", &out_program_options->arg_quiet_period_over_start},
        {"--stop-load", &out_program_options->arg_stop_load},
        {"--stop-interval", &out_program_options->arg_stop_interval},
        {"--quiet-period-over-stop", &out_program_options->arg_quiet_period_over_stop},
        {"--timeout", &out_program_options->arg_timeout}
    };

    for (int argument = 1; argument < argc; ++argument) {
        char* current_argument = argv[argument];
        // Setup simple options without arguments:
        if (strcmp(current_argument, "--help") == 0 || strcmp(current_argument, "-h") == 0) {
            show_help(out_program_options, argv);
            return OPTIONS_HELP;
        } else if (strcmp(current_argument, "--verbose") == 0
                   || strcmp(current_argument, "-v") == 0) {
            g_log.info.log = log_message;
            loadavgwatch_set_log_info(state, &g_log.info);
            out_program_options->verbose = true;
            continue;
        } else if (strcmp(current_argument, "--dry-run") == 0) {
            out_program_options->dry_run = true;
            continue;
        } else if (strcmp(current_argument, "--version") == 0) {
            show_version(out_program_options);
            return OPTIONS_VERSION;
        }
        bool known_option = false;
        // Setup argument values for options with arguments:
        for (int option_index = 0;
             option_index < sizeof(option_arguments) / sizeof(option_arguments[0]);
             ++option_index) {
            option_argument* current_option = &option_arguments[option_index];
            if (argument_name_matches(current_option->name, current_argument)) {
                if (!parse_option_argument(
                        current_option->name,
                        argc,
                        argv,
                        argument,
                        current_option->previous_value,
                        &argument)) {
                    return OPTIONS_FAILURE;
                }
                known_option = true;
                break;
            }
        }
        if (!known_option) {
            PRINTF_LOG_MESSAGE(
                g_log.error, "Unknown argument '%s'!", current_argument);
            return OPTIONS_FAILURE;
        }
    }

    timespec_argument timespec_arguments[] = {
        {"--start-interval",
         out_program_options->arg_start_interval,
         &out_program_options->start_interval},
        {"--quiet-period-over-start",
         out_program_options->arg_quiet_period_over_start,
         &out_program_options->quiet_period_over_start},
        {"--stop-interval",
         out_program_options->arg_stop_interval,
         &out_program_options->stop_interval},
        {"--quiet-period-over-stop",
         out_program_options->arg_quiet_period_over_stop,
         &out_program_options->quiet_period_over_stop},
        {"--timeout",
         out_program_options->arg_timeout,
         &out_program_options->timeout}
    };

    for (int timespec_index = 0;
         timespec_index < sizeof(timespec_arguments) / sizeof(timespec_arguments[0]);
         ++timespec_index) {

        timespec_argument* argument = &timespec_arguments[timespec_index];
        if (argument->value_str == NULL) {
            continue;
        }
        if (!_string_to_timespec(argument->value_str, argument->destination)) {
            PRINTF_LOG_MESSAGE(
                g_log.error,
                "'%s' is not a valid %s value!",
                argument->value_str,
                argument->name);
            return OPTIONS_FAILURE;
        }
    }

    if (out_program_options->arg_start_load != NULL) {
        assert(false && "TODO");
        loadavgwatch_set_start_load(
            state, &out_program_options->start_load);
    }
    if (out_program_options->arg_start_interval != NULL) {
        loadavgwatch_set_start_interval(
            state, &out_program_options->start_interval);
    }
    if (out_program_options->arg_quiet_period_over_start != NULL) {
        loadavgwatch_set_quiet_period_over_start(
            state, &out_program_options->quiet_period_over_start);
    }

    if (out_program_options->arg_stop_load != NULL) {
        assert(false && "TODO");
        loadavgwatch_set_stop_load(
            state, &out_program_options->stop_load);
    }
    if (out_program_options->arg_stop_interval != NULL) {
        loadavgwatch_set_stop_interval(
            state, &out_program_options->stop_interval);
    }
    if (out_program_options->arg_quiet_period_over_stop != NULL) {
        loadavgwatch_set_quiet_period_over_stop(
            state, &out_program_options->quiet_period_over_stop);
    }

    if (out_program_options->arg_timeout != NULL) {
        out_program_options->has_timeout = true;
    }
    return OPTIONS_OK;
}

static void run_command(
    const char* command, const char* child_action)
{
    PRINTF_LOG_MESSAGE(g_log.info, "Running %s", command);
    g_child_action = child_action;
    g_child_execution_warning_timeout = 10;
    alarm(10);
    int ret = system(command);
    alarm(0);
    if (ret != EXIT_SUCCESS) {
        PRINTF_LOG_MESSAGE(
            g_log.warning,
            "Child process exited with non-zero status %d.",
            ret);
    }
}

static const struct timespec* min_timespec(
    const struct timespec* left, const struct timespec* right)
{
    if (left->tv_sec < right->tv_sec) {
        return left;
    }
    if (right->tv_sec < left->tv_sec) {
        return right;
    }
    if (left->tv_nsec < right->tv_nsec) {
        return left;
    }
    return right;
}

static int monitor_and_act(
    loadavgwatch_state* state, program_options* options)
{
    // 3 pollings in 1 minute should result in high enough default
    // polling rate to catch relatively soon 1 minute load average
    // changes.
    static const struct timespec default_sleep_time = {
        .tv_sec = 20,
        .tv_nsec = 0
    };
    // Make sure that we don't sleep more than what makes it possible
    // to start new processes. TODO alternative algorithm to determine
    // if we should start new processes or not based on load
    // differential between previous measurement point:
    struct timespec sleep_time = *min_timespec(
        &default_sleep_time,
        min_timespec(
            &options->start_interval,
            &options->stop_interval));

    struct sigaction alarm_action = {
        .sa_sigaction = alarm_handler,
    };
    sigaction(SIGALRM, &alarm_action, NULL);

    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        PRINT_LOG_MESSAGE(
            g_log.error, "Unable to register program start time!");
        return EXIT_FAILURE;
    }
    struct timespec end_time = {
        .tv_sec = start_time.tv_sec + options->timeout.tv_sec };

    bool running = true;
    while (running) {
        loadavgwatch_poll_result poll_result;
        if (loadavgwatch_poll(state, &poll_result) != LOADAVGWATCH_OK) {
            abort();
        }
        if (poll_result.start_count > 0) {
            loadavgwatch_register_start(state);
            if (options->start_command != NULL) {
                if (options->dry_run) {
                    PRINTF_LOG_MESSAGE(
                        g_log.info, "Running: %s", options->start_command);
                } else {
                    run_command(options->start_command, "start");
                }
            }
        }
        if (poll_result.stop_count > 0) {
            loadavgwatch_register_stop(state);
            if (options->stop_command != NULL) {
                if (options->dry_run) {
                    PRINTF_LOG_MESSAGE(
                        g_log.info, "Running: %s", options->stop_command);
                } else {
                    run_command(options->stop_command, "stop");
                }
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
            PRINT_LOG_MESSAGE(g_log.info, "Timeout reached!");
            running = false;
            if (end_time.tv_sec <= now.tv_sec) {
                sleep_remaining.tv_sec = 0;
            } else {
                sleep_remaining.tv_sec = end_time.tv_sec - now.tv_sec;
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
        PRINT_LOG_MESSAGE(
            g_log.error,
            "Unable to close the library! This should never happen");
        program_result = EXIT_FAILURE;
    }
    return program_result;
}
