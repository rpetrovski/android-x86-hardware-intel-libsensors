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

// This is a "2D" faked compass.  The Intel sensor hub reports
// uncalibrated/raw magnetometer data because of Microsoft
// requirements for Windows sensor data.  Android has no calibration
// built into the frameworks and relies on the HAL to do it.  Which is
// sad, because that calibration is already being done internal to the
// sensor hub for use by the integrated orientation/quaternion
// sensors.
//
// Instead of doing the calibration again, we're just using that
// output quaternion by returning a faked "magnetometer" reading
// pointing directly at magnetic north.  This isn't quite correct, as
// real magnetic fields are 3D vectors with (typically) non-zero
// elevations.  The output from this sensor is always constrained to
// lie along the ground plane.

#define MAG_FIELD_UT 45 // Microtessla "field strength" reported

const struct sensor_t SynthCompassSensor::sSensorInfo_compass = {
    .name       = "HID_SENSOR Leveled Compass",
    .vendor     = "Intel",
    .version    = 1,
    .handle     = SENSORS_SYNCOMPASS_HANDLE,
    .type       = SENSOR_TYPE_MAGNETIC_FIELD,
    .maxRange   = MAG_FIELD_UT * 2,
    .resolution = MAG_FIELD_UT/256.0, // IIO reports 8 bit samples
    .power      = 0.1f,
    .minDelay   = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount      = 0,
    .reserved   = {},
};

SynthCompassSensor::SynthCompassSensor()
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_SC;
    mPendingEvent.type = SENSOR_TYPE_MAGNETIC_FIELD;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
}

// Converts a quaternion (need not be normalized) into a 3x3 rotation
// matrix.  Form and convetions are basically verbatim from Ken
// Shoemake's quatut.pdf.
static void quat2mat(const float q[4], float m[9])
{
    float x=q[0], y=q[1], z=q[2], w=q[3];
    float norm = 1/sqrt(x*x+y*y+z*z+w*w);
    x *= norm; y *= norm; z *= norm; w *= norm;
    float result[] = { 1-2*(y*y+z*z),   2*(x*y-w*z),   2*(x*z+w*y),
                         2*(x*y+w*z), 1-2*(x*x+z*z),   2*(y*z-w*x),
                         2*(x*z-w*y),   2*(y*z+w*x), 1-2*(x*x+y*y) };
    memcpy(m, result, 9*sizeof(float));
}

// Multiplies a 3-vector by a 3x3 matrix in row-major order
static void matmul(const float m[9], const float v[3], float out[3])
{
    float x=v[0], y=v[1], z=v[2];
    out[0] = x*m[0] + y*m[1] + z*m[2];
    out[1] = x*m[3] + y*m[4] + z*m[5];
    out[2] = x*m[6] + y*m[7] + z*m[8];
}

int SynthCompassSensor::enable(int en)
{
    if (!mRotVec)
        return -1;
    mRotVec->startStop(en);
    mEnabled = en;
    return 0;
}

int SynthCompassSensor::readEvents(sensors_event_t *data, int count)
{
    if (!mHasPendingEvent)
        return 0;

    mHasPendingEvent = false;
    mPendingEvent.timestamp = getTimestamp();
    *data = mPendingEvent;
    return 1;
}

void SynthCompassSensor::setQuaternion(float qin[4])
{
    if (!mEnabled)
        return;

    // The quat defines a rotation from device coordinates to world
    // coordinates, so invert it and generate a matrix that takes
    // world coords to device space.
    float q[4];
    memcpy(q, qin, sizeof(float)*4);
    q[0] *= -1;  q[1] *= -1; q[2] *= -1;

    float mat[9];
    quat2mat(q, mat);

    // "North" is alone the Y axis in world coordiantes, convert to
    // device space
    static const float v[] = { 0, MAG_FIELD_UT, 0 };
    matmul(mat, v, mPendingEvent.data);
    mPendingEvent.timestamp = getTimestamp();

    mHasPendingEvent = true;
}
