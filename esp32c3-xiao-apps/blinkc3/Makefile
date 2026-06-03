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

PROGNAME  = $(CONFIG_ESP32C3_XIAO_BLINKC3_PROGNAME)
PRIORITY  = $(CONFIG_ESP32C3_XIAO_BLINKC3_PRIORITY)
STACKSIZE = $(CONFIG_ESP32C3_XIAO_BLINKC3_STACKSIZE)
MODULE    = $(CONFIG_ESP32C3_XIAO_BLINKC3)

# blinkc3 

MAINSRC = blinkc3_main.c3
CSRCS = blinkc3_bridge.c

# Add your C3 object to the list of objects NuttX needs to link
# This ensures it's included in the final binary
MODULE_OBJS = blinkc3_main.o


include $(APPDIR)/Application.mk
