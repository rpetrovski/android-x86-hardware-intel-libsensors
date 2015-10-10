# Copyright (C) 2014 The Android-x86 Open Source Project
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

LOCAL_PATH := $(call my-dir)

# Common files.
common_src_files := sensors.cpp \
		    SensorBase.cpp \
		    SensorInputDev.cpp \
		    InputEventReader.cpp \
		    Helpers.cpp \
		    SensorIIODev.cpp \

# Board specific sensors.
sensor_src_files := HidSensor_Accel3D.cpp \
		    HidSensor_Gyro3D.cpp \
		    HidSensor_Compass3D.cpp \
		    HidSensor_ALS.cpp \
		    RotVecSensor.cpp \
		    SynthCompassSensor.cpp \
		    OrientationSensor.cpp \

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/bdw_rvp
LOCAL_MODULE := sensors.bdw_rvp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl
LOCAL_SRC_FILES := $(common_src_files) $(sensor_src_files) bdw_rvp/BoardConfig.cpp
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/bdw_wsb
LOCAL_MODULE := sensors.bdw_wsb
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl
LOCAL_SRC_FILES := $(common_src_files) $(sensor_src_files) bdw_wsb/BoardConfig.cpp
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/bytm
LOCAL_MODULE := sensors.bytm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl
LOCAL_SRC_FILES := $(common_src_files) $(sensor_src_files) bytm/BoardConfig.cpp
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/hsb
LOCAL_MODULE := sensors.hsb
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl
LOCAL_SRC_FILES := $(common_src_files) $(sensor_src_files) hsb/BoardConfig.cpp
include $(BUILD_SHARED_LIBRARY)
