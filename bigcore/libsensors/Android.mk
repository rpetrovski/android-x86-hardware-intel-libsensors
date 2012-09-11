# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# HAL module implemenation, not prelinked, and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.product.board>.so

ifeq ($(TARGET_BOARD_PLATFORM),bigcore)
ifeq ($(BOARD_USE_PLATFORM_SENSOR_LIB),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Common files.
common_src_path := ../../common/libsensors
common_src_files := $(common_src_path)/sensors.cpp \
		    $(common_src_path)/SensorBase.cpp \
		    $(common_src_path)/SensorInputDev.cpp \
		    $(common_src_path)/InputEventReader.cpp \
		    $(common_src_path)/Helpers.cpp \
		    $(common_src_path)/SensorIIODev.cpp \

# Board specific sensors.
sensor_src_files := $(common_src_path)/HidSensor_Accel3D.cpp \
		    $(common_src_path)/HidSensor_Gyro3D.cpp \
		    $(common_src_path)/HidSensor_Compass3D.cpp \
		    $(common_src_path)/HidSensor_ALS.cpp \

include external/stlport/libstlport.mk
LOCAL_C_INCLUDES += $(LOCAL_PATH) device/intel/common/libsensors

LOCAL_MODULE := sensors.$(TARGET_DEVICE)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libstlport
LOCAL_PRELINK_MODULE := false
LOCAL_SRC_FILES := $(common_src_files) $(sensor_src_files) BoardConfig.cpp

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_USE_PLATFORM_SENSOR_LIB == true
endif # TARGET_BOARD_PLATFORM == bigcore
