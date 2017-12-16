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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

static size_t _timespec_to_string(
    const struct timespec* value, char* out_result, size_t result_size)
{
    const time_t divisors[] = {24 * 60 * 60, 60 * 60, 60, 1};
    const char* labels[] = {"d", "h", "m", "s"};
    time_t remainder_seconds = value->tv_sec;
    if (remainder_seconds == 0) {
        size_t written = snprintf(out_result, result_size, "0s");
        return result_size - written;
    }
    char* output = out_result;
    ssize_t output_remaining = result_size;
    for (int i = 0; i < sizeof(divisors) / sizeof(divisors[0]); ++i) {
        time_t quotient = remainder_seconds / divisors[i];
        remainder_seconds %= divisors[i];
        if (quotient > 0) {
            size_t written = snprintf(
                output, output_remaining, "%ld%s", quotient, labels[i]);
            output_remaining -= written;
            output += written;
            if (output_remaining <= 0) {
                return result_size - output_remaining;
            }
        }
    }
    return result_size - output_remaining;
}

static bool _string_to_timespec(
    const char* time_str, struct timespec* out_result)
{
    const char labels[] = {'d', 'h', 'm', 's'};
    const time_t multipliers[] = {24 * 60 * 60, 60 * 60, 60, 1};

    size_t max_label_id = sizeof(labels) / sizeof(labels[0]);
    double current_seconds = 0;
    const char* current_start = time_str;
    // Skip over spaces before the first number:
    while (*current_start == ' ') {
        ++current_start;
    }
    const char* numbers_start = current_start;
    for (size_t label_id = 0;
         label_id < max_label_id && *current_start != '\0';
         label_id++) {
        char* endptr = NULL;
        double label_value = strtod(current_start, &endptr);
        if (label_value < 0.0) {
            return false;
        }
        if (endptr == current_start) {
            return false;
        }
        // Skip over spaces after numbers:
        while (*endptr == ' ') {
            ++endptr;
        }
        if (*endptr == '\0') {
            // We have a string that consists solely from
            // numbers. Treat it as seconds:
            if (current_start == numbers_start) {
                current_seconds = label_value;
                current_start = endptr;
                break;
            }
            // Otherwise we have a parse error if there are
            // non-numerical values in this string:
            return false;
        }
        while (label_id < max_label_id
               && tolower(*endptr) != labels[label_id]) {
            ++label_id;
        }
        if (label_id >= max_label_id) {
            return false;
        }
        current_seconds += label_value * multipliers[label_id];
        // Skip over spaces before numbers:
        while (*current_start == ' ') {
            ++current_start;
        }
        current_start = endptr + 1;
    }
    // Skip over spaces after the last number:
    while (*current_start == ' ') {
        ++current_start;
    }
    // There is something after the last known time unit label:
    if (*current_start != '\0') {
        return false;
    }
    // We got an empty string as timespec value:
    if (current_start == numbers_start) {
        return false;
    }
    out_result->tv_sec = current_seconds;
    // Adding 0.0000000005 to the result is poor man's rounding up to
    // avoid the x.xxx99999999... type of result without using the
    // math library that causes annoying extra linkage requirements on
    // Linux.
    out_result->tv_nsec = 1000000000 * (
        current_seconds - out_result->tv_sec + 0.0000000005);
    return true;
}
