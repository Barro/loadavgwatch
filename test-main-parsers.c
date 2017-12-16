#define _XOPEN_SOURCE 600

#include <assert.h>
#include "main-parsers.c"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ASSERT_TIMESPEC_TO_STRING_OUT(expected, seconds) \
    { \
        char out[32] = {0}; \
        struct timespec time = { .tv_sec = (seconds), .tv_nsec = 0 }; \
        _timespec_to_string(&time, out, sizeof(out)); \
        assert(strcmp(out, (expected)) == 0                       \
               && "Output time value did not match expected " expected); \
    }

#define ASSERT_STRING_TO_TIMESPEC_NSEC_OUT(sec, nsec, time_str)      \
    { \
        struct timespec time; \
        assert(_string_to_timespec(time_str, &time)); \
        assert(time.tv_sec == (sec) && time.tv_nsec == (nsec) \
               && "Seconds parsed from " time_str " did not match expected"); \
    }

#define ASSERT_STRING_TO_TIMESPEC_OUT(sec, time_str)   \
    { \
        struct timespec time; \
        assert(_string_to_timespec(time_str, &time)); \
        assert(time.tv_sec == (sec) && time.tv_nsec == 0 \
               && "Seconds parsed from " time_str " did not match expected"); \
    }

void test_timespec_to_string_should_be_able_to_output_all_time_units(void)
{
    ASSERT_TIMESPEC_TO_STRING_OUT("0s", 0);
    ASSERT_TIMESPEC_TO_STRING_OUT("1s", 1);
    ASSERT_TIMESPEC_TO_STRING_OUT("1m", 60);
    ASSERT_TIMESPEC_TO_STRING_OUT("1h", 60 * 60);
    ASSERT_TIMESPEC_TO_STRING_OUT("1d", 24 * 60 * 60);
    ASSERT_TIMESPEC_TO_STRING_OUT("1d1h1m1s", 24 * 60 * 60 + 60 * 60 + 60 + 1);
}

void test_string_to_timespec_should_be_able_to_parse_all_regular_time_units(void)
{
    ASSERT_STRING_TO_TIMESPEC_OUT(1, "1");
    ASSERT_STRING_TO_TIMESPEC_OUT(1, "1s");
    ASSERT_STRING_TO_TIMESPEC_OUT(60, "1m");
    ASSERT_STRING_TO_TIMESPEC_OUT(60 * 60, "1h");
    ASSERT_STRING_TO_TIMESPEC_OUT(24 * 60 * 60, "1d");
    ASSERT_STRING_TO_TIMESPEC_OUT(24 * 60 * 60 + 60 * 60 + 60 + 1, "1d1h1m1s");
}

void test_string_to_timespec_should_be_able_to_parse_more_exotic_time_representations(void)
{
    ASSERT_STRING_TO_TIMESPEC_OUT(1, " 1 ");
    ASSERT_STRING_TO_TIMESPEC_OUT(1, " 1s");
    ASSERT_STRING_TO_TIMESPEC_OUT(1, " 1 s");
    ASSERT_STRING_TO_TIMESPEC_OUT(1, " 1 s ");
    ASSERT_STRING_TO_TIMESPEC_OUT(1, "1.0s");
    ASSERT_STRING_TO_TIMESPEC_OUT(79 * 60, "79m");
    ASSERT_STRING_TO_TIMESPEC_OUT(2.5 * 60, "2.5m");
    ASSERT_STRING_TO_TIMESPEC_NSEC_OUT(1, 200000000, "1.2s");
}

int main()
{
    test_timespec_to_string_should_be_able_to_output_all_time_units();
    test_string_to_timespec_should_be_able_to_parse_all_regular_time_units();
    test_string_to_timespec_should_be_able_to_parse_more_exotic_time_representations();
    return EXIT_SUCCESS;
}
