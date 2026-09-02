############################################################################
# Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

#  built-in application info

PROGNAME  = $(CONFIG_ESP32C3_XIAO_WATCH_CLEANER_CONTROLLER_PROGNAME)
PRIORITY  = $(CONFIG_ESP32C3_XIAO_WATCH_CLEANER_CONTROLLER_PRIORITY)
STACKSIZE = $(CONFIG_ESP32C3_XIAO_WATCH_CLEANER_CONTROLLER_STACKSIZE)
MODULE    = $(CONFIG_ESP32C3_XIAO_WATCH_CLEANER_CONTROLLER)

# Source files 

MAINSRC = watch_cleaner_controller_main.c

CXXEXT = .cpp
CSRCS +=  motorcontroller.c motor_driver.c util.c gui.c
CXXSRCS += wcc_main_screen.cpp wcc_settings.cpp wcc_styles.cpp
MODULE_OBJS = watch_cleaner_controller_main.o motorcontroller.o motor_driver.o util.o gui.o wcc_main_screen.o wcc_settings.o wcc_styles.o

include $(APPDIR)/Application.mk
