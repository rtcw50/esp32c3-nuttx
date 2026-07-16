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
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/pwm.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include "motcontrol.h"

extern int textui_loop(int argc, char *argv[]);
extern int mc_task_entry(int argc, char *argv[]);

int g_ramp_time=2;         /* ramp up/down time in secs */
int g_speed=MOTOR_MAX_SPEED/2;          /* motor speed in rpm */
int g_duty = ((MOTOR_MAX_SPEED/2) * MOTOR_MAX_DUTY) / MOTOR_MAX_SPEED; /* duty cycle percentage */
int g_duration=300;        /* duration in secs to run motor */
int g_countdown;           /* countdown in secs */
int g_agitate_duration=10; /* duration of CW and CCW motion in secs 
                           one-half CW, one-half CCW */


// Set now to current time 
int mc_get_now(struct timespec *now)
{
    int rv = clock_gettime(CLOCK_MONOTONIC, now);
    if (rv == -1) {
        return -1;
    }
    return 0;
}

// Set an absolute time in the future wait_ms from current time
int mc_get_abstime_from_now(struct timespec *future_time, long wait_ms)
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

int motcontrol_main(int argc, FAR char *argv[])
{
    
/* Main App Initialization */
if (board_app_initialize(0) < 0) {
    return -1;
}    

#if 1
if (motor_driver_init() != 0) {
    fprintf(stderr, "Error: Failed to initialize motor driver.\n");
    return -1;
}
#endif

/* 1. Pre-create the Command Queue (UI -> Motor) */
static const struct mq_attr cleaner_cmd_attr = {
    .mq_maxmsg = 4,
    .mq_msgsize = sizeof(struct clean_cmd_msg_s),
    .mq_flags = 0
};
mq_unlink("/cleaner_cmd_q"); /* ignore ENOENT */
mqd_t setup_cmd = mq_open("/cleaner_cmd_q", O_CREAT | O_RDWR, 0666, &cleaner_cmd_attr);
if (setup_cmd == (mqd_t)-1)
{
    perror("mq_open /cleaner_cmd_q");
}
else
{
    mq_close(setup_cmd);
}

/* 2. Pre-create the Telemetry Queue (Motor -> UI) */
static const struct mq_attr cleaner_tel_attr = {
    .mq_maxmsg = 4,
    .mq_msgsize = sizeof(struct clean_tel_msg_s),
    .mq_flags = 0
};
mq_unlink("/cleaner_tel_q");
mqd_t setup_tel = mq_open("/cleaner_tel_q", O_CREAT | O_RDWR, 0666, &cleaner_tel_attr);
if (setup_tel == (mqd_t)-1)
{
    perror("mq_open /cleaner_tel_q");
}
else
{
    mq_close(setup_tel);
}

/* Now it is 100% safe to launch your tasks */
task_create("motor_task", 150, 2048, mc_task_entry, NULL);
task_create("ui_task",    100, 16384, textui_loop,    NULL);

// Don't shutdown the PWMs, they should be active for the life of the application.
#if 0
motor_driver_shutdown(); /* Shutdown the motor driver hardware */
#endif

return 0;
} 
