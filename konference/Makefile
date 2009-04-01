# $Id: Makefile 889 2007-08-09 14:42:48Z sbalea $

#
# Makefile, based on the Asterisk Makefile, Coypright (C) 1999, Mark Spencer
#
# Copyright (C) 2002,2003 Junghanns.NET GmbH
#
# Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
#
# This program is free software and may be modified and
# distributed under the terms of the GNU Public License.
#

.EXPORT_ALL_VARIABLES:

#
# app_konference defines which can be passed on the command-line
#

INSTALL_PREFIX :=
INSTALL_MODULES_DIR := $(INSTALL_PREFIX)/usr/lib/asterisk/modules

ASTERISK_INCLUDE_DIR ?= ../asterisk/include

REVISION = $(shell svnversion -n .)

# turn app_konference debugging on or off ( 0 == OFF, 1 == ON )
APP_CONFERENCE_DEBUG ?= 0

# 0 = OFF 1 = astdsp 2 = speex
SILDET := 2

#
# app_konference objects to build
#

OBJS = app_conference.o conference.o member.o frame.o cli.o
TARGET = app_konference.so


#
# standard compile settings
#

PROC = $(shell uname -m)
INSTALL = install

INCLUDE = -I$(ASTERISK_INCLUDE_DIR)
DEBUG := -g

CFLAGS = -pipe -Wall -Wmissing-prototypes -Wmissing-declarations -MD -MP $(DEBUG)
CPPFLAGS = $(INCLUDE) -D_REENTRANT -D_GNU_SOURCE -DREVISION=\"$(REVISION)\"
#CFLAGS += -O2
#CFLAGS += -O3 -march=pentium3 -msse -mfpmath=sse,387 -ffast-math
# PERF: below is 10% faster than -O2 or -O3 alone.
#CFLAGS += -O3 -ffast-math -funroll-loops
# below is another 5% faster or so.
#CFLAGS += -O3 -ffast-math -funroll-all-loops -fsingle-precision-constant
#CFLAGS += -mcpu=7450 -faltivec -mabi=altivec -mdynamic-no-pic
# adding -msse -mfpmath=sse has little effect.
#CFLAGS += -O3 -msse -mfpmath=sse
#CFLAGS += $(shell if $(CC) -march=$(PROC) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-march=$(PROC)"; fi)
CFLAGS += $(shell if uname -m | grep -q ppc; then echo "-fsigned-char"; fi)
CFLAGS += -fPIC
CPPFLAGS += -DCRYPTO

#
# Uncomment this if you want G.729A support (need to have the actual codec installed
#
# CPPFLAGS += -DAC_USE_G729A


ifeq ($(APP_CONFERENCE_DEBUG), 1)
CPPFLAGS += -DAPP_CONFERENCE_DEBUG
endif

#
# additional flag values for silence detection
#

ifeq ($(SILDET), 2)
OBJS += libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o
CPPFLAGS += -Ilibspeex -DSILDET=2
endif

ifeq ($(SILDET), 1)
CPPFLAGS += -DSILDET=1
endif

OSARCH=$(shell uname -s)
ifeq (${OSARCH},Darwin)
SOLINK=-dynamic -bundle -undefined suppress -force_flat_namespace
else
SOLINK=-shared -Xlinker -x
endif

DEPS += $(subst .o,.d,$(OBJS))

#
# targets
#

all: $(TARGET)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(DEPS)

.PHONY: distclean
distclean: clean
	$(RM) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -pg $(SOLINK) -o $@ $(OBJS)

vad_test: vad_test.o libspeex/preprocess.o libspeex/misc.o libspeex/smallft.o
	$(CC) $(PROFILE) -o $@ $^ -lm

install:
	$(INSTALL) -m 755 $(TARGET) $(INSTALL_MODULES_DIR)


# config: all
# 	cp conf.conf /etc/asterisk/

-include $(DEPS)

