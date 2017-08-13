LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mididrone
LOCAL_CATEGORY_PATH := tools
LOCAL_DESCRIPTION := Play a MIDI file on the drone
LOCAL_SRC_FILES := \
	mididrone.cpp \
	stdout_driver.cpp \
	pwm_driver.cpp
LOCAL_LIBRARIES := portsmf linux
LOCAL_CFLAGS := -std=gnu99
LOCAL_CXXFLAGS := -std=c++0x
include $(BUILD_EXECUTABLE)

