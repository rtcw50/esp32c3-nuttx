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
#include "watch_cleaner_controller.h"

// Set now to current time 
int wcc_get_now(struct timespec *now)
{
    int rv = clock_gettime(CLOCK_MONOTONIC, now);
    if (rv == -1) {
        return -1;
    }
    return 0;
}

// Set an absolute time in the future wait_ms from current time
int wcc_get_abstime_from_now(struct timespec *future_time, long wait_ms)
{
    clock_gettime(CLOCK_MONOTONIC, future_time);
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