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

PROGNAME  = $(CONFIG_ESP32C3_XIAO_HELLOC3_PROGNAME)
PRIORITY  = $(CONFIG_ESP32C3_XIAO_HELLOC3_PRIORITY)
STACKSIZE = $(CONFIG_ESP32C3_XIAO_HELLOC3_STACKSIZE)
MODULE    = $(CONFIG_ESP32C3_XIAO_HELLOC3)

# helloc3 

MAINSRC = helloc3_main.c3

# Add your C3 object to the list of objects NuttX needs to link
# This ensures it's included in the final binary
MODULE_OBJS = helloc3_main.o

# Future for linking
EXTOBJS := libc.o  libc.os.o std.collections.map.o std.core.ascii.o std.core.builtin.o \
std.core.dstring.o  std.core.mem.allocator.o  std.core.mem.allocators.o  std.core.mem.o \
std.core.runtime.o  std.core.string.conv.o  std.core.string.o  std.core.types.o \
std.io.file.o  std.io.o  std.io.os.o  std.math.o  compiler_rt.o


# Weak, undefined symbols from the application that need to 
# be pulled in from the standard library are captured here. This is necessary
# because the standard library is built with weak symbols and the linker will only pull in 
# the object files from the library that are needed to resolve undefined symbols in the application. 
#C3_STDLIB_LDFLAGS := -u std.io.stdout -u std.io.file.from_handle -u std.io.file.from_raw_handle -u std.io.file.from_raw_fd
C3_STDLIB_LDFLAGS := $(foreach file, $(EXTOBJS),$(call GET_UNDEFINED_SYMBOLS, $(file)))

# Note we write the undefined symbol list to a common directory and a specific application file
# The file is read by the arch/risc-v/src/Makefile to ensure the symbols are included in the final link.
$(shell mkdir -p $(APPDIR)$(DELIM)c3_ldflags)
$(shell echo "$(C3_STDLIB_LDFLAGS)" > $(APPDIR)$(DELIM)c3_ldflags/$(PROGNAME).ldflags)

$(EXTOBJS):
	$(Q) touch $@


include $(APPDIR)/Application.mk
