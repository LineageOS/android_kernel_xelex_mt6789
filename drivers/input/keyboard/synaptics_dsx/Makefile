#
# Makefile for the Synaptics DSX touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-y += -Wno-error
obj-m += synaptics_dsx_keyboard.o

synaptics_dsx_keyboard-y := synaptics_dsx_i2c.o synaptics_dsx_core.o synaptics_dsx_fw_update.o
