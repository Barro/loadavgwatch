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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include "loadavgwatch-linux-parsers.c"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_CLOSE(expected, actual) \
    { \
    bool small_value_matches = fabs(actual) < 0.0001 && fabs((expected) - (actual)) < 0.0001; \
    bool large_value_matches = fabs((expected) - (actual)) / fabs(actual) < 0.0001; \
    if (!(small_value_matches || large_value_matches)) { \
        fprintf(stderr, "Expected value " #expected " == %lf does not match the actual " #actual " == %lf!\n", (expected), (actual)); \
        assert(false && #expected "!=" #actual); \
    } \
    }

static FILE* memfile_from_string(const char* value)
{
    size_t file_size = strlen(value);
    FILE* result = fmemopen((char*)value, file_size, "r");
    assert(result != NULL);
    return result;
}


void test_valid_proc_loadavg_should_produce_expected_result(void)
{
    FILE* loadavg_fp = memfile_from_string("0.01 0.02 0.03 4/5 6");
    float read_loadavg = -1.0;
    assert(LOADAVGWATCH_OK == _get_load_average_proc_loadavg(
               loadavg_fp, &read_loadavg));
    fclose(loadavg_fp);
    ASSERT_CLOSE(0.01, read_loadavg);
}

void test_invalid_proc_loadavg_should_produce_error_and_not_modify_result(void)
{
    FILE* loadavg_fp = memfile_from_string("asdf");
    float read_loadavg = -1.0;
    assert(LOADAVGWATCH_OK != _get_load_average_proc_loadavg(
               loadavg_fp, &read_loadavg));
    fclose(loadavg_fp);
    ASSERT_CLOSE(-1.0, read_loadavg);
}

int main()
{
#if !defined(__APPLE__) || __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__  >= 101300
    test_valid_proc_loadavg_should_produce_expected_result();
    test_invalid_proc_loadavg_should_produce_error_and_not_modify_result();
#else
    fprintf(stderr, "OS X supports fmemopen only at 10.13!\n");
#endif
    return EXIT_SUCCESS;
}
