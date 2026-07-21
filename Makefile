# =========================================================
# atto-miner build system
# =========================================================
# Two independent axes, both overridable on the command line:
#
#   ARCH=linux|windows  which HAL backend / toolchain to build with
#                       (default: linux)
#   MODE=normal|minimal
#                       normal  = dynamic, interactive (asks for
#                                 username/mining key at runtime)
#                       minimal = static, credentials baked in at
#                                 compile time, silent (LOG is a
#                                 no-op) — for routers/daemons
#                       (default: normal)
#
# Examples:
#   make                             # linux, normal  -> atto-miner-v1.0.0-linux-x64
#   make ARCH=windows                # windows, normal -> atto-miner-v1.0.0-windows-gcc-x64.exe
#   make MODE=minimal USERNAME=foo MINING_KEY=bar RIG_ID=rig1
#                                    # linux, minimal -> atto-miner-v1.0.0-linux-static-x64
#   make ARCH=windows MODE=minimal USERNAME=foo MINING_KEY=bar
#                                    # windows, minimal -> atto-miner-v1.0.0-windows-gcc-static-x64.exe
# =========================================================

ARCH ?= linux
MODE ?= normal

HAL_DIR := hal

ifeq ($(ARCH),linux)
    CC        := gcc
    CC_MUSL   := musl-gcc
    HAL       := $(HAL_DIR)/hal_linux.c
    LIBS      := -pthread
    OS_TAG    := linux
    ARCH_TAG  := x64
    EXE_EXT   :=
else ifeq ($(ARCH),windows)
    CC        := x86_64-w64-mingw32-gcc
    HAL       := $(HAL_DIR)/hal_windows.c
    LIBS      := -lws2_32
    OS_TAG    := windows
    ARCH_TAG  := x64
    EXE_EXT   := .exe
else
    $(error invalid ARCH: '$(ARCH)'. Use ARCH=linux or ARCH=windows)
endif

COMMON_FLAGS := -Wall -Wno-stringop-overread -s -I$(HAL_DIR)

ifeq ($(MODE),normal)
    FLAGS := $(COMMON_FLAGS) -O3
    MODE_FLAGS :=
    EXECUTABLE := atto-miner-$(OS_TAG)-$(ARCH_TAG)$(EXE_EXT)
else ifeq ($(MODE),minimal)
    ifneq ($(ARCH),windows)
        CC := $(CC_MUSL)
    endif
    FLAGS := $(COMMON_FLAGS) -Os -ffunction-sections -fdata-sections -flto
    MODE_FLAGS := -static -DMINIMAL \
        -Wl,--gc-sections \
        -DDUCO_USERNAME=\"$(USERNAME)\" \
        -DDUCO_MINING_KEY=\"$(MINING_KEY)\" \
        -DDUCO_RIG_ID=\"$(RIG_ID)\" \
        -DDUCO_THREADS=$(THREADS)
    EXECUTABLE := atto-miner-$(OS_TAG)-static-$(ARCH_TAG)$(EXE_EXT)
else
    $(error Invalid Mode: '$(MODE)'. Use MODE=normal or MODE=minimal)
endif

# NOTE: Do not edit credentials directly in this file.
# Pass them via the command line instead. Example:
# make ARCH=windows MODE=minimal THREADS=4 USERNAME=myuser MINING_KEY=mykey RIG_ID=myrig

USERNAME ?= "username"
MINING_KEY ?= "password"
RIG_ID ?= "atto-static"
THREADS ?= 1
SOURCES := main.c sha1.c $(HAL)

.PHONY: all clean help size
all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES) $(HAL_DIR)/hal.h sha1.h
	$(CC) $(SOURCES) $(FLAGS) $(MODE_FLAGS) $(LIBS) -o $(EXECUTABLE)

size: $(EXECUTABLE)
	@size $(EXECUTABLE)
	@ls -la $(EXECUTABLE)

clean:
	rm -f *.o atto-miner-*

help:
	@echo "make [ARCH=linux|windows] [MODE=normal|minimal] [USERNAME=...] [MINING_KEY=...] [RIG_ID=...] [THREADS=...]"
