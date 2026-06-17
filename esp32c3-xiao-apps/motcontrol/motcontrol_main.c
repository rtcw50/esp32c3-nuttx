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

extern int textui_loop(int);
int g_ramp_time=2; /* ramp up/down time in secs */
int g_speed=300;     /* motor speed in rpm */
int g_duration=300;  /* duration in secs to run motor */
int g_agitate_duration=10; /* duration of CW and CCW motion in secs 
                           one-half CW, one-half CCW */
int g_mode=0; /* running, stopped, paused */


int motcontrol_main(int argc, FAR char *argv[])
{
    volatile int debughold = 1; /* set to 0 to allow main loop to run */
    //while (debughold) {
    //    usleep(100000);
   // }

    int fd = -1;
    int nopen_tries = 50;
    usleep(300000); /* wait for ACM0 to be ready */
    while ((fd < 0) && nopen_tries > 0) {
        fd = open("/dev/ttyACM0", O_RDWR|O_NOCTTY);
        if (fd < 0) {
            usleep(100000); /* wait and try again */
        }
        nopen_tries--;
    }
    if (fd < 0) {
        printf("Failed to open /dev/ttyACM0\n");
        return 1;
    }
    usleep(500000); /* wait for USB handshake to complete */ 

    // Duplicate raw ACM0 descriptor to stdin, stdout, stderr
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
	int ret = textui_loop(fd);
    close(fd);
	return ret;
} 
