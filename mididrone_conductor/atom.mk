LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mididrone_conductor
LOCAL_CATEGORY_PATH := mididrone
LOCAL_DESCRIPTION := Synchronization source for one or more mididrone_musicians.
LOCAL_SRC_FILES := \
	mididrone_conductor.c

LOCAL_CFLAGS := -std=gnu99

include $(BUILD_EXECUTABLE)

