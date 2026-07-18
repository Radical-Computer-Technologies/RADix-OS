#
# RADKernel Circle Pi Zero 2 W smoke image
#

RADPX_OS_ROOT ?= ../../..
RADLIB_ROOT ?= $(RADPX_OS_ROOT)/../RADLib
CIRCLEHOME ?= /media/jvincent/Kingspec512/repos/circle

AARCH ?= 64
RASPPI ?= 3
STDLIB_SUPPORT ?= 3
STANDARD ?= -std=c++20
QEMU ?= 0
CPPFLAGS += -fno-rtti

RADCORE_OBJS = \
	radcore_RADCore.o \
	radcore_RADEventBackend.o \
	radcore_RADEventLoop.o \
	radcore_RADThread.o \
	radcore_RADTimer.o \
	radcore_RADIODevice.o \
	radcore_RADJson.o \
	radcore_RADSystem.o \
	radcore_RADLog.o

RADLIB_OBJS = \
	raddsp_RADDsp.o \
	radsettings_RADSettings.o \
	radlogging_RADLogging.o \
	radserial_RADSerial.o \
	raddevice_RADDevice.o

OBJS = main.o kernel.o radboot_wrap.o radkernel_runtime_wrap.o radkernel_overlay_wrap.o radkernel_hal_circle_wrap.o radkernel_runtime_newlib_wrap.o \
	$(RADCORE_OBJS) $(RADLIB_OBJS)

EXTRAINCLUDE += -I$(RADPX_OS_ROOT)/RADKernel/headers \
	-I$(RADPX_OS_ROOT)/RADKernel/boot \
	-I$(RADLIB_ROOT)/RADCore/headers \
	-I$(RADLIB_ROOT)/RADDsp/headers \
	-I$(RADLIB_ROOT)/RADSettings/headers \
	-I$(RADLIB_ROOT)/RADLogging/headers \
	-I$(RADLIB_ROOT)/RADSerial/headers \
	-I$(RADLIB_ROOT)/RADDevice/headers \
	-I$(RADLIB_ROOT)
DEFINE += -DRADLIB_EMBEDDED_NO_CPP_WRAPPERS=1 \
	-DRADLIB_EMBEDDED=1 \
	-DRADLIB_LICENSE_ACCEPTED=1 \
	-DRADLIB_LICENSE_TIER_INTERNAL=1 \
	-DRADLIB_EDITION_COMMUNITY=1 \
	-DRADLIB_EDITION_REQUIRES_LICENSE_KEY=0

ifeq ($(QEMU),1)
DEFINE += -DNO_SDHOST -DNO_SCREEN_DMA_BURST_LENGTH -DLEAVE_QEMU_ON_HALT
endif

LIBS = $(CIRCLEHOME)/lib/sched/libsched.a \
	$(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/sample/Rules.mk

LDFLAGS += --allow-multiple-definition
EXTRALIBS += "$(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libc.a)" \
	"$(shell $(PREFIX)gcc $(ARCHCPU) -print-file-name=libm.a)"

radcore_RADCore.o: $(RADLIB_ROOT)/RADCore/source/RADCore.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADEventBackend.o: $(RADLIB_ROOT)/RADCore/source/RADEventBackend.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADEventLoop.o: $(RADLIB_ROOT)/RADCore/source/RADEventLoop.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADThread.o: $(RADLIB_ROOT)/RADCore/source/RADThread.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADTimer.o: $(RADLIB_ROOT)/RADCore/source/RADTimer.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADIODevice.o: $(RADLIB_ROOT)/RADCore/source/RADIODevice.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADJson.o: $(RADLIB_ROOT)/RADCore/source/RADJson.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADSystem.o: $(RADLIB_ROOT)/RADCore/source/RADSystem.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radcore_RADLog.o: $(RADLIB_ROOT)/RADCore/source/RADLog.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

raddsp_RADDsp.o: $(RADLIB_ROOT)/RADDsp/source/RADDsp.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radsettings_RADSettings.o: $(RADLIB_ROOT)/RADSettings/source/RADSettings.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radlogging_RADLogging.o: $(RADLIB_ROOT)/RADLogging/source/RADLogging.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

radserial_RADSerial.o: $(RADLIB_ROOT)/RADSerial/source/RADSerial.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

raddevice_RADDevice.o: $(RADLIB_ROOT)/RADDevice/source/RADDevice.cpp
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)
