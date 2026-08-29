
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
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/pwm.h>

#define PWM_MAX_DUTY 95 

static int fd0, fd1;
static struct pwm_info_s pwm1info, pwm2info;
static int active_pwm = 1; // 0 for pwm1, 1 for pwm2

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/* The device is configured to use Timer 1 and Timer 2 connected to PWM1, PWM2 
  respectively. Each PWM has 1 channel enabled. Pin assignments to PWM1,2
  is handled via the configuration header (config.h via make menuconfig). 
  This device uses /dev/pwm1 -> Pin D1/GPIO3 and /dev/pwm2 -> Pin D2/GPIO4.
  To change the pin assignments, use make menuconfig and select the appropriate 
  pins for PWM1 and PWM2.
 */
static void configure_pwm_info(struct pwm_info_s *info, int frequency, int duty)
{
  info->frequency = frequency;
  info->channels[0].channel = 0; // Pin D1/GPIO3 
  info->channels[0].duty = (b16divi(uitoub16(duty), 100)); 
  info->channels[0].cpol = PWM_CPOL_HIGH; // Active high
  info->channels[0].dcpol = PWM_DCPOL_LOW; // Disabled channel

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int wcc_motor_driver_init(void)
{
  /* Initialize PWM info: 20kHz, 13-bit resolution (0-8191) */
  memset(&pwm1info, 0, sizeof(struct pwm_info_s)); // Clear the structure first
  memset(&pwm2info, 0, sizeof(struct pwm_info_s)); // Clear the structure first

  (void)configure_pwm_info(&pwm1info, /* frequency */ 1220, /* duty */ 0);
  (void)configure_pwm_info(&pwm2info, /* frequency */ 1220, /* duty */ 0);

  fd0 = open("/dev/pwm1", O_RDONLY);
  fd1 = open("/dev/pwm2", O_RDONLY);

  if (fd0 < 0 || fd1 < 0) {
    return -1;
  }

  /* Set initial characteristics  - initally off or stopped */
  int ret;
  ret = ioctl(fd0, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm1info));
  if (ret < 0) {
    return -1;
  } 
  ret = ioctl(fd1, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm2info));
  if (ret < 0) {
    return -1;
  } 
  return 0;
}

void wcc_motor_driver_shutdown(void)
{
  ioctl(fd0, PWMIOC_STOP, 0);
  ioctl(fd1, PWMIOC_STOP, 0);
  close(fd0);
  close(fd1);
}

int wcc_motor_driver_start_pwm(void)
{
  //pwm1info.channels[0].duty = (b16divi(uitoub16(0), 0)); 
  //pwm2info.channels[0].duty = (b16divi(uitoub16(0), 0)); 

  int ret;
  ret = ioctl(fd0, PWMIOC_START, 0);
  if (ret < 0) {
    return -1;
  }
  ret = ioctl(fd1, PWMIOC_START, 0);
  if (ret < 0) {
    return -1;
  }
  return 0;
}

void wcc_motor_driver_set_duty(int duty)
{
    if (active_pwm == 0) {
        //pwm1info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
        // Set duty cycle in range 0-8191 with 13-bit resolution and an addend to round up to nearest integer
        pwm1info.channels[0].duty = (int32_t)((((uint64_t)duty * 
          ((1 << CONFIG_ESPRESSIF_LEDC_TIMER1_RESOLUTION) - 1)) + 50) / 100);
        // Negate the implicit right shift by 3 bits
        pwm1info.channels[0].duty = pwm1info.channels[0].duty << 3;
        ioctl(fd0, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm1info));
    } else {
        //pwm2info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
        pwm2info.channels[0].duty = (int32_t)((((uint64_t)duty * 
          ((1 << CONFIG_ESPRESSIF_LEDC_TIMER2_RESOLUTION) - 1)) + 50) / 100);
        pwm2info.channels[0].duty = pwm2info.channels[0].duty << 3;
        ioctl(fd1, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm2info));
    }
}

void wcc_motor_driver_reverse_motor_direction(void)
{
    active_pwm = 1 - active_pwm; // Toggle between 0 and 1
}