APPLICATION = ttn-test

BOARD ?=lora-e5-dev

RIOTBASE ?= $(CURDIR)/../RIOT

USEMODULE += sx126x_stm32wl
USEMODULE += netdev_default
USEMODULE += shell ps #xtimer
USEMODULE += fmt periph_i2c
USEMODULE += semtech_loramac_rx
USEMODULE += bme280_i2c

USEMODULE += periph_flashpage periph_flashpage_pagewise
# https://doc.riot-os.org/group__sys__ztimer.html
USEMODULE += ztimer
USEMODULE += shell_commands
USEMODULE += ztimer_sec 

#set right i2c address for our bme280
CFLAGS += -DBMX280_PARAM_I2C_DEV=I2C_DEV\(0\)
CFLAGS += -DBMX280_PARAM_I2C_ADDR=0x76

FEATURE_REQUIRED += periph_uart

USEPKG += semtech-loramac


LORA_REGION ?= EU868

DEVELHELP = 1

include $(RIOTBASE)/Makefile.include