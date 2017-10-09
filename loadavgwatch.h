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

#ifndef LOADAVGWATCH_H
#define LOADAVGWATCH_H

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

#include <stdint.h>

typedef enum loadavgwatch_status
{
    LOADAVGWATCH_OK = 0,
    LOADAVGWATCH_ERR_OUT_OF_MEMORY = -1,
    LOADAVGWATCH_ERR_INVALID_PARAMETER = -2,
    LOADAVGWATCH_ERR_INIT = -3,
    LOADAVGWATCH_ERR_READ = -4,
    LOADAVGWATCH_ERR_PARSE = -5,
    LOADAVGWATCH_ERR_CLOCK = -6
} loadavgwatch_status;

typedef struct loadavgwatch_state loadavgwatch_state;

typedef struct loadavgwatch_init_parameter
{
    const char* key;
    void* value;
} loadavgwatch_init_parameter;

typedef struct loadavgwatch_poll_result
{
    uint32_t start_count;
    uint32_t stop_count;
} loadavgwatch_poll_result;

typedef struct loadavgwatch_log_object
{
    void(*log)(const char* message, void* data);
    void* data;
} loadavgwatch_log_object;

loadavgwatch_status loadavgwatch_open(
    const loadavgwatch_init_parameter* parameters,
    loadavgwatch_state** out_state);
loadavgwatch_status loadavgwatch_close(loadavgwatch_state** state);
loadavgwatch_status loadavgwatch_poll(
    loadavgwatch_state* state, loadavgwatch_poll_result* result);
loadavgwatch_status loadavgwatch_register_start(loadavgwatch_state* state);
loadavgwatch_status loadavgwatch_register_stop(loadavgwatch_state* state);

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#endif // #ifndef LOADAVGWATCH_H
