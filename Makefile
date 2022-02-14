APPLICATION = ttn-test

BOARD ?=lora-e5-dev

RIOTBASE ?= $(CURDIR)/../RIOT

USEMODULE += sx126x_stm32wl
USEMODULE += netdev_default
USEMODULE += xtimer shell ps
USEMODULE += fmt periph_i2c 

FEATURE_REQUIRED += periph_uart

USEPKG += semtech-loramac

USEMODULE += shell_commands

LORA_REGION ?= EU868

DEVELHELP = 1

include $(RIOTBASE)/Makefile.include