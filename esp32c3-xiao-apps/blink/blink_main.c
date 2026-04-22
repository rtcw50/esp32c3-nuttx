/****************************************************************************
 * apps/examples/blink/blink_main.c
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

#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include <nuttx/ioexpander/gpio.h>


/* Define GPIO ioctl macros locally to ensure availability */

/* See *nuttx/boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_gpio.c */
//#define BLUE_LED 0 /* index 0 -> /dev/gpio0 -> D1 */
//#define RED_LED  1 /* index 1 -> /dev/gpio1 -> D2 */   
#define BLUE_LED 2 /* index 0 -> /dev/gpio0 -> D3 */
#define RED_LED  3 /* index 1 -> /dev/gpio1 -> D4 */   

#define ENABLE_RED_LED 1 


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * blink_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int fd1; 
  #if ENABLE_RED_LED
  int fd2;
  #endif    
  char devpath[16];
  int ret;

  printf("Blink: Toggling GPIO BLUE LED...\n");
  #if ENABLE_RED_LED
  printf("Blink: Toggling GPIO RED LED...\n");
  #endif

  /* Open GPIO device files directly */
  snprintf(devpath, sizeof(devpath), "/dev/gpio%d", BLUE_LED);
  fd1 = open(devpath, O_RDWR);
  #if ENABLE_RED_LED
  snprintf(devpath, sizeof(devpath), "/dev/gpio%d", RED_LED);
  fd2 = open(devpath, O_RDWR);
  #endif

  if (fd1 < 0 ) {
  
      printf("ERROR: Failed to open GPIO devices - BLUE LED\n");
      return -1;
  }
  #if ENABLE_RED_LED
  if ( fd2 < 0) {
  
      printf("ERROR: Failed to open GPIO devices - RED LED\n");
      return -1;
  }
  #endif


  /* Configure pins as outputs */
  ret = ioctl(fd1, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);
  if (ret < 0) {
      printf("ERROR: ioctl GPIOC_SETPINTYPE failed for BLUE LED (errno: %d)\n", errno);
      close(fd1);
  #if ENABLE_RED_LED
      close(fd2);
  #endif
      return -1;
  }
  #if ENABLE_RED_LED
  ret = ioctl(fd2, GPIOC_SETPINTYPE, GPIO_OUTPUT_PIN);
  if (ret < 0) {
      printf("ERROR: ioctl GPIOC_SETPINTYPE failed for GPIO 9 (errno: %d)\n", errno);
      close(fd1);
      close(fd2);
      return -1;
  }
  #endif

  /* Toggle 10 times*/
  for (int i = 0; i < 10; i++) {
      /* Set GPIO 10 High, GPIO 9 Low */
      ret = ioctl(fd1, GPIOC_WRITE, (unsigned long)1);
      if (ret < 0) printf("ERROR: GPIOC_WRITE high failed for BLUE LED (errno: %d)\n", errno);
#if ENABLE_RED_LED
      ret = ioctl(fd2, GPIOC_WRITE, (unsigned long)0);
      if (ret < 0) printf("ERROR: GPIOC_WRITE low failed for RED LED (errno: %d)\n", errno);
#endif
      usleep(500000);    

      /* Set GPIO 10 Low, GPIO 9 High */
      ret = ioctl(fd1, GPIOC_WRITE, (unsigned long)0);
      if (ret < 0) printf("ERROR: GPIOC_WRITE low failed for BLUE LED (errno: %d)\n", errno);
#if ENABLE_RED_LED
      ret = ioctl(fd2, GPIOC_WRITE, (unsigned long)1);
      if (ret < 0) printf("ERROR: GPIOC_WRITE high failed for RED LED (errno: %d)\n", errno);
#endif

      usleep(500000);   
  }

  close(fd1);
#if ENABLE_RED_LED
  close(fd2);
#endif

  return 0;
}
