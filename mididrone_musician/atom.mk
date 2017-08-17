LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mididrone_musician
LOCAL_CATEGORY_PATH := mididrone
LOCAL_DESCRIPTION := Play a MIDI file on the drone, receiving instructions from the conductor
LOCAL_SRC_FILES := \
	mididrone_musician.cpp \
	stdout_driver.cpp

LOCAL_LIBRARIES := portsmf libpomp libfutils
LOCAL_FORCE_STATIC := 1
LOCAL_CFLAGS := -std=gnu99
LOCAL_CXXFLAGS := -std=c++0x

ifeq ("$(TARGET_CPU)","p6i")
LOCAL_DEPENDS_HEADERS := linux
LOCAL_CFLAGS += -DUSE_MINIDRONES_PWM_DRIVER

LOCAL_SRC_FILES += \
	pwm_driver.cpp
endif

include $(BUILD_EXECUTABLE)

