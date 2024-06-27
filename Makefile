# name of your application
APPLICATION = example-app

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Uncomment these lines if you want to use platform support from external
# repositories:
#RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
#EXTERNAL_BOARD_DIRS ?= $(CURDIR)/../../RIOT/thirdparty_boards

# Uncomment this to enable scheduler statistics for ps:
USEMODULE += schedstatistics

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

WERROR ?=0

# select MicroPython package
USEPKG += micropython
USEPKG += lua

# include boot.py as header
BLOBS += boot.py
BLOBS += $(wildcard *.lua)

# Modules to include:
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
# include and auto-initialize all available sensors
# USEMODULE += saul_default

USEMODULE += uuid
USEMODULE += xtimer
# USEMODULE += ztimer

USEMODULE += progress_bar

# Use a network interface, if available. The handling is done in
# Makefile.board.dep, which is processed recursively as part of the dependency
# resolution.
# FEATURES_OPTIONAL += netif

FEATURES_OPTIONAL += periph_rtc

FEATURES_REQUIRED += periph_gpio_irq

FEATURES_REQUIRED += cpp # basic C++ support
FEATURES_REQUIRED += libstdcpp # libstdc++ support (for #include <cstdio>)


ifneq (,$(filter msba2,$(BOARD)))
  USEMODULE += mci
  USEMODULE += random
endif

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
