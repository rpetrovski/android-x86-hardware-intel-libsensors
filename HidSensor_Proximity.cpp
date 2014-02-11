/*
 * Copyright (C) 2014 The Android Open Source Project
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
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>

#include "common.h"
#include "SensorConfig.h"
#include "HidSensor_Proximity.h"


struct prox_sample{
    unsigned short presence;
} __packed;

const struct sensor_t ProximitySensor::sSensorInfo_proximity = {
    "HID_SENSOR Proximity", "Intel", 1, SENSORS_PROXIMITY_HANDLE, SENSOR_TYPE_PROXIMITY,
        5.0f, 1.0f, 0.75f, 0, 0, 0, {}
    ,
};
const int retry_cnt = 10;

ProximitySensor::ProximitySensor(): SensorIIODev("prox", "in_proximity_scale", "in_proximity_offset", "in_proximity_", retry_cnt){
    ALOGV(">>ProximitySensor 3D: constructor!");
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_P;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

    // CDD is silent on Proximity requirements.  Fix default at 2Hz pending
    // better numbers from somewhere.
     sample_delay_min_ms = 500;

    ALOGV("<<ProximitySensor 3D: constructor!");
}

int ProximitySensor::processEvent(unsigned char *raw_data, size_t raw_data_len){
    struct prox_sample *sample;

    ALOGV(">>%s", __func__);
    if (IsDeviceInitialized() == false){
        ALOGE("Device was not initialized \n");
        return  - 1;
    } if (raw_data_len < sizeof(struct prox_sample)){
        ALOGE("Insufficient length \n");
        return  - 1;
    }
    sample = (struct prox_sample*)raw_data;
    if (sample->presence == 1)
            mPendingEvent.distance = (float)0;
    else
            mPendingEvent.distance = (float)5;


    ALOGE("Proximity %fm\n", mPendingEvent.distance);
    ALOGV("<<%s", __func__);
    return 0;
}
