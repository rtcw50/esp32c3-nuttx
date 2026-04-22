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

#define PWM_MAX_DUTY 95 

/****************************************************************************
 * Private Functions
 ****************************************************************************/
/* Device is configured to use Timer 0 and Timer 1 connected to PWM0, PWM1 
  respectively. Each PWM has 1 channel enabled. Pin assignments to PWM0,1
  is handled via the configuration header. This device uses /dev/pwm0 -> Pin D1/GPIO3
  and /dev/pwm1 -> Pin D2/GPIO4.*/
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

int main(int argc, FAR char *argv[])
{
  struct pwm_info_s pwm0info, pwm1info  ;
  int fd0, fd1;
  int frequency = 1000; // 1 kHz
  int duty = 0; // Initially off or stopped motor

  /* Initialize PWM info: 20kHz, 13-bit resolution (0-8191) */
  memset(&pwm0info, 0, sizeof(struct pwm_info_s)); // Clear the structure first
  memset(&pwm1info, 0, sizeof(struct pwm_info_s)); // Clear the structure first

  (void)configure_pwm_info(&pwm0info, frequency, duty);
  (void)configure_pwm_info(&pwm1info, frequency, duty);


  printf("PWM Pipe-cleaner: Starting 13-bit ramp on /dev/pwm0 and /dev/pwm1\n");

  fd0 = open("/dev/pwm0", O_RDONLY);
  fd1 = open("/dev/pwm1", O_RDONLY);

  if (fd0 < 0 || fd1 < 0) {
    printf("Error: Could not open PWM devices.\n");
    return -1;
  }

  /* Set initial characteristics  - initally off or stopped */
  int ret;
  ret =ioctl(fd0, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm0info));
  if (ret < 0) {
    printf("Error: Could not set characteristics for /dev/pwm0: %d.\n", errno);
    return -1;
  } 
  ret = ioctl(fd1, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm1info));
  if (ret < 0) {
    printf("Error: Could not set characteristics for /dev/pwm1: %d.\n", errno);
    return -1;
  } 

  /* Start the timers */
  ret = ioctl(fd0, PWMIOC_START, 0);
  if (ret < 0) {
    printf("Error: Could not start /dev/pwm0: %d.\n", errno);
    return -1;
  }
  ret = ioctl(fd1, PWMIOC_START, 0);
  if (ret < 0) {
    printf("Error: Could not start /dev/pwm1: %d.\n", errno);
    return -1;
  }
  // Small wait time to ensure the timers have started before we begin ramping
  usleep(50000);

  /* Simple Ramp State Machine: Channel 0 (D1/GPIO3) */
  printf("Ramping GPIO3 PWM Pin (D1) ...\n");
  for (duty = 0; duty <= PWM_MAX_DUTY; duty += 1) {
//    pwm0info.channels[0].duty = (b16divi(uitoub16(duty) - 1, 100)); 
    pwm0info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
    ioctl(fd0, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm0info));

    usleep(20000);
  }
  // Run for a few seconds at full speed
  sleep(3); 

  int duty_ch0 = duty; // Store current duty for channel 0 before ramping down

  /* Ramp down */
  printf("Ramping down GPIO3 PWM Pin (D1) ...\n");
  for (duty = duty_ch0; duty >= 0; duty -= 1) {
//    pwm0info.channels[0].duty = (b16divi(uitoub16(duty) - 1, 100)); 
    pwm0info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
    ioctl(fd0, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm0info));
    usleep(20000);
  } 


   /* Simple Ramp State Machine: Channel 1 (D2/GPIO4) */
  printf("Ramping GPIO4 PWM Pin (D2) ...\n");
  for (duty = 0; duty <= PWM_MAX_DUTY; duty += 1) {
 //   pwm1info.channels[0].duty = (b16divi(uitoub16(duty) - 1, 100)); 
    pwm1info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
    ioctl(fd1, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm1info));
    usleep(20000);
  }
  // Run for a few seconds at full speedi
  sleep(3); 
  int duty_ch1 = duty; // Store current duty for channel 1 before ramping down

  /* Ramp down */
  printf("Ramping down GPIO4 PWM Pin (D2) ...\n");
  for (duty = duty_ch1; duty >= 0; duty -= 1) {
//    pwm1info.channels[0].duty = (b16divi(uitoub16(duty) - 1, 100)); 
    pwm1info.channels[0].duty = (uint32_t)(((uint64_t)duty << 16) / 100);
    ioctl(fd1, PWMIOC_SETCHARACTERISTICS, (unsigned long)((uintptr_t)&pwm1info));
    usleep(20000);
  }  

  ioctl(fd0, PWMIOC_STOP, 0);
  ioctl(fd1, PWMIOC_STOP, 0);


  /* Clean up */
  close(fd0);
  close(fd1);

  printf("Pipe-cleaner finished.\n");
  return 0;
}
