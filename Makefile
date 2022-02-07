APPLICATION = ttn-test

BOARD ?=lora-e5-dev

RIOTBASE ?= $(CURDIR)/../RIOT

USEMODULE += sx126x_stm32wl
USEMODULE += netdev_default
USEMODULE += xtimer shell

USEPKG += semtech-loramac

LORA_REGION ?= EU868

DEVELHELP = 1

include $(RIOTBASE)/Makefile.include