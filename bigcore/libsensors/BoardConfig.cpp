/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <cutils/log.h>

#include "common.h"

#include "BoardConfig.h"
#include "SensorConfig.h"
#include "HidSensor_Accel3D.h"
#include "HidSensor_Gyro3D.h"
#include "HidSensor_Compass3D.h"
#include "HidSensor_ALS.h"

static const struct sensor_t sSensorList[] = {
    AccelSensor::sSensorInfo_accel3D,
    GyroSensor::sSensorInfo_gyro3D,
    CompassSensor::sSensorInfo_compass3D,
    ALSSensor::sSensorInfo_als,
};

const struct sensor_t* BoardConfig::sensorList()
{
    return sSensorList;
}

int BoardConfig::sensorListSize()
{
    return ARRAY_SIZE(sSensorList);
}

void BoardConfig::initSensors(SensorBase* sensors[])
{
    sensors[accel] = new AccelSensor();
    sensors[gyro] = new GyroSensor();
    sensors[compass] = new CompassSensor();
    sensors[light] = new ALSSensor();
}

int BoardConfig::handleToDriver(int handle)
{
    switch (handle) {
    case ID_A:
        return accel;
    case ID_M:
        return compass;
    case ID_PR:
    case ID_T:
        return -EINVAL;
    case ID_GY:
        return gyro;
    case ID_L:
        return light;
  default:
        return -EINVAL;
    }
    return -EINVAL;
}
