LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mididrone_splitter
LOCAL_CATEGORY_PATH := mididrone
LOCAL_DESCRIPTION := Split a MIDI file into several other MIDI files, one per drone.
LOCAL_SRC_FILES := \
	musician.cpp \
	dispatcher.cpp \
	mididrone_splitter.cpp

LOCAL_LIBRARIES := portsmf libpomp libfutils
LOCAL_CFLAGS := -std=gnu99
LOCAL_CXXFLAGS := -std=c++0x

include $(BUILD_EXECUTABLE)

