/****************************************************************************
 * 
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <unistd.h>
#include <time.h>
#include <nuttx/mqueue.h>
#include "watch_cleaner_controller.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Get the current time
 * 
 * Retrieves the current monotonic clock time and stores it in the provided timespec structure.
 * 
 * @param now Pointer to a timespec structure where the current time will be stored
 * @return int 0 on success, -1 on error (errno will be set)
 * 
 * @note Uses CLOCK_MONOTONIC which is unaffected by system clock adjustments
 */
int wcc_get_now(struct timespec *now)
{
    int rv = clock_gettime(CLOCK_REALTIME, now);
    if (rv == -1) {
        return -1;
    }
    return 0;
}

/**
 * @brief Calculate an absolute time in the future
 * 
 * Computes an absolute deadline by adding the specified number of milliseconds
 * to the current monotonic time.
 * 
 * @param future_time Pointer to a timespec structure where the calculated future time will be stored
 * @param wait_ms Number of milliseconds to add to current time (must be non-negative)
 * @return int 0 on success, -1 on error
 * 
 * @note Handles overflow of nanoseconds by carrying over to seconds
 * @note Uses MC_MS_PER_SEC and MC_NS_PER_MS constants from header file
 */
int wcc_get_abstime_from_now(struct timespec *future_time, long wait_ms)
{
    // CLOCK_MONOTONIC 
    clock_gettime(CLOCK_REALTIME, future_time);
    if (wait_ms <= MC_MS_PER_SEC) {
        future_time->tv_nsec += (wait_ms * MC_NS_PER_MS);
    }
    else {
        future_time->tv_sec += wait_ms / MC_MS_PER_SEC;
        future_time->tv_nsec += (wait_ms % MC_MS_PER_SEC) * MC_NS_PER_MS;
    }
    if (future_time->tv_nsec >= MC_NS_PER_SEC) { 
        future_time->tv_sec += 1;
        future_time->tv_nsec -= MC_NS_PER_SEC;
    }
    return 0;
}
 
/**
 * @brief Compare two timespec structures
 * 
 * Compares two absolute times to determine their relative ordering.
 * 
 * @param a Pointer to the first timespec structure
 * @param b Pointer to the second timespec structure
 * @return int -1 if a < b, 0 if a == b, 1 if a > b
 * 
 * @note Returns -1 if a is earlier than b, 1 if a is later than b, 0 if equal
 */
int wcc_timespec_compare(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec < b->tv_sec) {
        return -1;
    }
    if (a->tv_sec > b->tv_sec) {
        return 1;
    }
    if (a->tv_nsec < b->tv_nsec) {
        return -1;
    }
    if (a->tv_nsec > b->tv_nsec) {
        return 1;
    }
    return 0;
}

/**
 * @brief Find the minimum deadline from multiple timespec values
 * 
 * Determines the earliest (minimum) time from three provided timespec structures.
 * 
 * @param t1 Pointer to the first timespec structure
 * @param t2 Pointer to the second timespec structure
 * @param t3 Pointer to the third timespec structure
 * @return struct timespec The earliest (minimum) time among the three inputs
 * 
 * @note Returns a copy of the minimum timespec, not a pointer
 * @note Uses wcc_timespec_compare for comparison
 */
struct timespec wcc_get_min_deadline(struct timespec *t1, struct timespec *t2, struct timespec *t3)
{
    struct timespec min = *t1;
    if (wcc_timespec_compare(t2, &min) < 0) {
        min = *t2;
    }
    if (wcc_timespec_compare(t3, &min) < 0) {
        min = *t3;
    }
    return min;
}

void wcc_timespec_add_ms(struct timespec *ts, long ms)
{
    ts->tv_sec += ms / MC_MS_PER_SEC;
    ts->tv_nsec += (ms % MC_MS_PER_SEC) * MC_NS_PER_MS;
    if (ts->tv_nsec >= MC_NS_PER_SEC) {
        ts->tv_sec += 1;
        ts->tv_nsec -= MC_NS_PER_SEC;
    }
}

long wcc_elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    long elapsed_sec = end->tv_sec - start->tv_sec;
    long elapsed_nsec = end->tv_nsec - start->tv_nsec;
    return (elapsed_sec * MC_MS_PER_SEC) + (elapsed_nsec / MC_NS_PER_MS);
}

void ui_send_cmd(mqd_t *q, uint16_t msg_type,uint16_t value)
{
    struct clean_cmd_msg_s cmd_msg;
    cmd_msg.msg_type = msg_type;
    cmd_msg.value = value;
    mq_send(*q, (void*)&cmd_msg, sizeof(struct clean_cmd_msg_s), 0);  
}   
#ifdef __cplusplus
}
#endif
