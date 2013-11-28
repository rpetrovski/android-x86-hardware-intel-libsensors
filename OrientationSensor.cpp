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
#include "OrientationSensor.h"

const struct sensor_t OrientationSensor::sSensorInfo_orientation = {
    .name       = "HID_SENSOR Orientation",
    .vendor     = "Intel",
    .version    = 1,
    .handle     = SENSORS_ORIENTATION_HANDLE,
    .type       = SENSOR_TYPE_ORIENTATION,
    .maxRange   = 360.0,
    .resolution = 1./512, // Advertise as 9 bits, no idea about reality
    .power      = 0.1f,
    .minDelay   = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount      = 0,
    .reserved   = {},
};

OrientationSensor::OrientationSensor()
    : SensorIIODev("incli_3d",  // name
                   "in_incli_scale",  // units sysfs node
                   "in_incli_offset", // exponent sysfs node
                   "in_incli_",       // channel_prefix
                   10)                // retry count
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_O;
    mPendingEvent.type = SENSOR_TYPE_ORIENTATION;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    sample_delay_min_ms = 50; // 20Hz default
}

int OrientationSensor::processEvent(unsigned char *data, size_t len)
{
    if (IsDeviceInitialized() == false) {
        ALOGE("Device was not initialized \n");
        return -1;
    }

    if (len < 3*sizeof(unsigned int)) {
        ALOGE("Insufficient length \n");
        return -1;
    }

    float vals[3];
    unsigned int *sample = (unsigned int*)data;
    long ex = GetExponentValue();
    for (int i=0; i<3; i++) {
        int sz = GetChannelBytesUsedSize(i);
        vals[i] = convert_from_vtf_format(sz, ex, sample[i]);
    }

    // When held in the Android convention frame (X to the right, Y
    // toward "screen up", Z out from the screen toward the user) a
    // windows sensor hub reports pitch/roll/yaw where Android wants
    // hdg/pitch/roll.  And the sense of the rotations is reversed
    // from what is observed on Nexus devices (which themselves seem
    // to disagree with documentation on the sign of the output, see:
    // http://developer.android.com/reference/android/hardware/SensorEvent.html#values).
    //
    // These conversions produce behavior identical to the Nexus 7,
    // Galaxy Nexus and Google Edition GS4:
    mPendingEvent.data[0] = 360.0 - vals[2];
    mPendingEvent.data[1] = -vals[0];
    mPendingEvent.data[2] = -vals[1];

    ALOGV("orient %f %f %f\n", mPendingEvent.data[0], mPendingEvent.data[1], mPendingEvent.data[2]);

    return 0;
}
