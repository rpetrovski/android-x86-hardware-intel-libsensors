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

#ifndef HIDSENSOR_SYNTHCOMPASS_H
#define HIDSENSOR_SYNTHCOMPASS_H

#include "SensorIIODev.h"
#include "RotVecSensor.h"

class RotVecSensor;

class SynthCompassSensor : public SensorBase {

public:
    SynthCompassSensor();

    void setRotVecSensor(RotVecSensor *s) { mRotVec = s; }

    // From the RotationVector sensor
    void setQuaternion(float q[4]);

    // From SensorBase:
    virtual int open() { return mRotVec != NULL; }
    virtual void close() {}
    virtual int enable(int en);
    virtual int readEvents(sensors_event_t *data, int count);
    virtual int setDelay(int64_t ns) { return mRotVec ? mRotVec->setDelay(ns) : -1; }

    static const struct sensor_t sSensorInfo_compass;

 private:
    RotVecSensor *mRotVec;
};
#endif
