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

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "loadavgwatch-linux-parsers.c"
#include "main-parsers.c"

#define AFL_PERSISTENT_ITERATIONS 2000

void fuzz_one(FILE* input_fp)
{
        char input_type;
        if (fread(&input_type, 1, 1, input_fp) != 1) {
            const char error_message[] = "Input was a zero length string!\n";
            fwrite(error_message, sizeof(error_message), 1, stderr);
            return;
        }

        switch (input_type)
        {
        case '1': {
            float loadavg;
            _get_load_average_proc_loadavg(input_fp, &loadavg);
            break;
        }
        case '2':
            _get_ncpus_proc_cpuinfo(input_fp);
            break;
        case '3':
            _get_ncpus_sys_devices(input_fp);
            break;
        case '4': {
            struct timespec timespec;
            if (fread(&timespec, sizeof(timespec), 1, input_fp) != 1) {
                break;
            }
            char result[32] = {0};
            _timespec_to_string(&timespec, result, sizeof(result));
            break;
        }
        case '5': {
            struct timespec result;
            char buffer[32];
            size_t items = fread(buffer, 1, sizeof(buffer) - 1, input_fp);
            buffer[items] = '\0';
            _string_to_timespec(buffer, &result);
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        const char error_message[] = "Need to give an input file as the first parameter!\n";
        fwrite(error_message, sizeof(error_message), 1, stderr);
        return EXIT_FAILURE;
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif // #ifdef __AFL_HAVE_MANUAL_CONTROL

#ifdef __AFL_LOOP
    while (__AFL_LOOP(AFL_PERSISTENT_ITERATIONS)) {
#else // #ifdef __AFL_LOOP
    bool iterating = true;
    while (iterating) {
        iterating = false;
#endif // #ifdef __AFL_LOOP

        FILE* input_fp = fopen(argv[1], "r");
        if (input_fp == NULL) {
            perror("Unable to open input file");
            return EXIT_FAILURE;
        }

        fuzz_one(input_fp);
        fclose(input_fp);
    }

    return EXIT_SUCCESS;
}
