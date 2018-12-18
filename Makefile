# Teensyduino Core Library
# http://www.pjrc.com/teensy/
# Copyright (c) 2017 PJRC.COM, LLC.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# 1. The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# 2. If the Software is incorporated into a build system that allows
# selection among a list of target devices, then similar target
# devices manufactured by PJRC.COM must be included in the list of
# target devices and selectable in the same manner.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# set MCU and LOWER_MCU here
MCU=MK66FX1M0
LOWER_MCU=mk66fx1m0

MCU_LD = $(LOWER_MCU).ld

# The name of your project (used to name the compiled .hex file)
TARGET = main

# configurable options
OPTIONS = -DF_CPU=120000000 -DUSB_MTPDISK -DLAYOUT_US_ENGLISH

# options needed by many Arduino libraries to configure for Teensy 3.0
OPTIONS += -D__$(MCU)__ -DARDUINO=10613 -DTEENSYDUINO=132

# Other Makefiles and project templates for Teensy 3.x:
#
# https://github.com/apmorton/teensy-template
# https://github.com/xxxajk/Arduino_Makefile_master
# https://github.com/JonHylands/uCee

#************************************************************************
# Settings below this point usually do not need to be edited
#************************************************************************

# CPPFLAGS = compiler options for C and C++
CPPFLAGS = -Wall -g -Os -mcpu=cortex-m4 -mthumb -MMD $(OPTIONS) -I.
CPPFLAGS += -I./i2c_t3
#CPPFLAGS += -I./utility
CPPFLAGS += -I./Time
CPPFLAGS += -I./SdFs/src

# compiler options for C++ only
CXXFLAGS = -std=gnu++0x -felide-constructors -fno-exceptions -fno-rtti

# compiler options for C only
CFLAGS =

# linker options
LDFLAGS = -Os -Wl,--gc-sections,--defsym=__rtc_localtime=0 --specs=nano.specs -mcpu=cortex-m4 -mthumb -T$(MCU_LD)

# additional libraries to link
LIBS = -lm

# names for the compiler programs
CC = ../toolchain/bin/arm-none-eabi-gcc
CXX = ../toolchain/bin/arm-none-eabi-g++
OBJCOPY = ../toolchain/bin/arm-none-eabi-objcopy
SIZE = ../toolchain/bin/arm-none-eabi-size

# automatically create lists of the sources and objects
C_FILES := $(wildcard *.c)
#C_FILES += $(wildcard utility/*.c)
C_FILES += $(wildcard Time/*.c)
C_FILES += $(wildcard SdFs/*.c)

CPP_FILES := $(wildcard *.cpp)
#CPP_FILES += $(wildcard utility/*.cpp)
CPP_FILES += ./i2c_t3/i2c_t3.cpp
CPP_FILES += $(wildcard Time/*.cpp)
CPP_FILES += $(wildcard SdFs/*.cpp)
CPP_FILES += $(wildcard SdFs/src/common/*.cpp)
CPP_FILES += $(wildcard SdFs/src/*.cpp)
CPP_FILES += $(wildcard SdFs/src/SdCard/*.cpp)
CPP_FILES += $(wildcard SdFs/src/SpiDriver/*.cpp)
CPP_FILES += $(wildcard SdFs/src/ExFatLib/*.cpp)
CPP_FILES += $(wildcard SdFs/src/FatLib/*.cpp)


OBJS := $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o)

# the actual makefile rules (all .o files built by GNU make's default implicit rules)

all: $(TARGET).hex

$(TARGET).elf: $(OBJS) $(MCU_LD)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.hex: %.elf
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# compiler generated dependency info
-include $(OBJS:.o=.d)

clean:
	rm -f *.o *.d $(TARGET).elf $(TARGET).hex

.PHONY: upload
upload: all
	teensy_loader_cli -v -w --mcu=$(LOWER_MCU) $(TARGET).hex
