#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := subscribe_publish

EXTRA_COMPONENT_DIRS = /home/leszek/github/esp-aws-iot
#EXTRA_COMPONENT_DIRS := $(realpath ../)

include $(IDF_PATH)/make/project.mk

