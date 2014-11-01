/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cutils/log.h>
#include "common.h"
#include "SensorConfig.h"
#include "SynthCompassSensor.h"
#include "RotVecSensor.h"

const struct sensor_t RotVecSensor::sSensorInfo_rotvec = {
    .name       = "HID_SENSOR Rotation Vector",
    .vendor     = "Intel",
    .version    = 1,
    .handle     = SENSORS_ROT_VEC_HANDLE,
    .type       = SENSOR_TYPE_ROTATION_VECTOR,
    .maxRange   = 1.0,
    .resolution = 1./512, // Advertise as 9 bits, no idea about reality
    .power      = 0.1f,
    .minDelay   = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount      = 0,
    .stringType = SENSOR_STRING_TYPE_ROTATION_VECTOR,
    .requiredPermission     = "",
    .maxDelay   = 0,
    .flags      = SENSOR_FLAG_CONTINUOUS_MODE,
    .reserved   = {},
};

RotVecSensor::RotVecSensor()
    : SensorIIODev("dev_rotation",  // name
                   "in_rot_scale",  // units sysfs node
                   "in_rot_offset", // exponent sysfs node
                   "in_rot_",       // channel_prefix
                   10)              // retry count
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_R;
    mPendingEvent.type = SENSOR_TYPE_ROTATION_VECTOR;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    sample_delay_min_ms = 50; // 20Hz default

    mSynthCompass = NULL;
}

int RotVecSensor::processEvent(unsigned char *data, size_t len)
{
    if (IsDeviceInitialized() == false) {
        ALOGE("Device was not initialized \n");
        return -1;
    }

    if (len < 4*sizeof(unsigned int)) {
        ALOGE("Insufficient length \n");
        return -1;
    }

    // The Intel Sensor Hub emits a normalized x/y/z/w quaternion
    // which acts to rotate points in the device coordinate system
    // (left/up/out) to the world coordinate system (north/east/down).
    // This is pleasingly identical to the Android convention, so just
    // copy out the raw data.

    unsigned int *sample = (unsigned int*)data;
    long ex = GetExponentValue();
    for (int i=0; i<4; i++) {
        int sz = GetChannelBytesUsedSize(i);
        mPendingEvent.data[i] = convert_from_vtf_format(sz, ex, sample[i]);
    }

    if (mSynthCompass)
        mSynthCompass->setQuaternion(mPendingEvent.data);

    return 0;
}
