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
#include <time.h>

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

typedef struct _loadavgwatch_state loadavgwatch_state;

typedef struct loadavgwatch_parameter
{
    const char* key;
    void* value;
} loadavgwatch_parameter;

typedef struct loadavgwatch_load
{
    uint32_t load;
    uint32_t scale;
} loadavgwatch_load;

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

loadavgwatch_status loadavgwatch_open(loadavgwatch_state** out_state);
loadavgwatch_status loadavgwatch_open_logging(
    loadavgwatch_state** out_state,
    loadavgwatch_log_object* log_warning,
    loadavgwatch_log_object* log_error);
loadavgwatch_status loadavgwatch_set_log_info(
    loadavgwatch_state* state, loadavgwatch_log_object* log);
loadavgwatch_status loadavgwatch_set_log_warning(
    loadavgwatch_state* state, loadavgwatch_log_object* log);
loadavgwatch_status loadavgwatch_set_log_error(
    loadavgwatch_state* state, loadavgwatch_log_object* log);
loadavgwatch_status loadavgwatch_set_start_load(
    loadavgwatch_state* state, const loadavgwatch_load* load);
loadavgwatch_status loadavgwatch_set_start_interval(
    loadavgwatch_state* state, const struct timespec* interval);
loadavgwatch_status loadavgwatch_set_quiet_period_over_start(
    loadavgwatch_state* state, const struct timespec* interval);
loadavgwatch_status loadavgwatch_set_stop_load(
    loadavgwatch_state* state, const loadavgwatch_load* load);
loadavgwatch_status loadavgwatch_set_start_interval(
    loadavgwatch_state* state, const struct timespec* interval);
loadavgwatch_status loadavgwatch_set_quiet_period_over_start(
    loadavgwatch_state* state, const struct timespec* interval);

const char* loadavgwatch_get_system(const loadavgwatch_state* state);
loadavgwatch_load loadavgwatch_get_start_load(const loadavgwatch_state* state);
struct timespec loadavgwatch_get_start_interval(
    const loadavgwatch_state* state);
struct timespec loadavgwatch_get_quiet_period_over_start(
    const loadavgwatch_state* state);
loadavgwatch_load loadavgwatch_get_stop_load(const loadavgwatch_state* state);
struct timespec loadavgwatch_get_stop_interval(
    const loadavgwatch_state* state);
struct timespec loadavgwatch_get_quiet_period_over_stop(
    const loadavgwatch_state* state);

loadavgwatch_status loadavgwatch_close(loadavgwatch_state** state);
loadavgwatch_status loadavgwatch_poll(
    loadavgwatch_state* state, loadavgwatch_poll_result* result);
loadavgwatch_status loadavgwatch_register_start(loadavgwatch_state* state);
loadavgwatch_status loadavgwatch_register_stop(loadavgwatch_state* state);

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#endif // #ifndef LOADAVGWATCH_H
