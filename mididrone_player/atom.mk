LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mididrone_player
LOCAL_CATEGORY_PATH := mididrone
LOCAL_DESCRIPTION := Play a MIDI file on the drone
LOCAL_SRC_FILES := \
	mididrone_player.cpp \
	stdout_driver.cpp \
	pwm_driver.cpp
LOCAL_LIBRARIES := portsmf
LOCAL_DEPENDS_HEADERS := linux
LOCAL_CFLAGS := -std=gnu99
LOCAL_CXXFLAGS := -std=c++0x
include $(BUILD_EXECUTABLE)

