/*
 * Copyright (C) 2010-2012 Intel Corporation
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
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "SensorIIODev.h"
#include "Helpers.h"

using namespace std;

static const std::string IIO_DIR = "/sys/bus/iio/devices";
static const int DEF_BUFFER_LEN = 2;
static const int DEF_HYST_VALUE = 0;

SensorIIODev::SensorIIODev(const std::string& dev_name, const std::string& scale,
                           const std::string& offset,
                           const std::string& channel_prefix, int retry_cnt)
             : SensorBase(""),
               initialized(false),
               scale_str(scale),
               offset_str(offset),
               device_name(dev_name),
               channel_prefix_str(channel_prefix),
               scale(0.0),
               offset(0.0),
               retry_count(retry_cnt),
               raw_buffer(NULL),
               mRefCount(0),
               sample_delay_min_ms(0)
{
    ALOGV("%s", __func__);
}

int SensorIIODev::discover()
{
    int cnt;
    int status;
    int ret;

    // Allow overriding sample_delay_min_ms via properties using
    // the IIO device name.  e.g.:
    //     ro.sys.sensors_accel_3d_samp_min_ms = 16
    char sampmin[PROPERTY_VALUE_MAX];
    std::string pn = "ro.sys.sensors_" + device_name + "_samp_min_ms";
    property_get(pn.c_str(), sampmin, "");
    if(*sampmin)
        sample_delay_min_ms = strtol(sampmin, NULL, 10);

    ALOGD(">>discover %s", device_name.c_str());
    for (cnt = 0; cnt < retry_count; cnt++) {
        status = ParseIIODirectory(device_name);
        if (status >= 0){
            std::stringstream filename;
            initialized = true;
            filename << "/dev/iio:device" << device_number;
            mDevPath = filename.str();
            ALOGI("found %s at %s", device_name.c_str(), mDevPath.c_str());
            ret = 0;
            break;
        } else {
            ALOGE("%s not found, retry left: %d", device_name.c_str(), retry_count-cnt);
            mDevPath = "";
            ret = -1;
        }
        // Device not enumerated yet, wait and retry
        sleep(1);
    }
    return ret;
}

int SensorIIODev::AllocateRxBuffer()
{
    ALOGV(">>%s Allocate:%d", __func__, datum_size * buffer_len);
    raw_buffer = new unsigned char[datum_size * buffer_len];
    if (!raw_buffer) {
        ALOGE("AllocateRxBuffer: Failed\n");
        return -ENOMEM;
    }
    return 0;
}

int SensorIIODev::FreeRxBuffer()
{
    ALOGV(">>%s:%s:", device_name.c_str(), __func__);
    delete []raw_buffer;
    raw_buffer = NULL;
    ALOGV("<<%s:%s:", device_name.c_str(), __func__);
    return 0;
}

int SensorIIODev::enable(int enabled)
{
    // startStop() handles the internal sensor stream, which may be
    // used by slaves.  The mEnabled flag is what gates the output of
    // this particular sensor.
    int ret = startStop(enabled);
    if (ret)
        return ret;
    mEnabled = enabled;
    return 0;
}

/**
 * When enabled == true, 0 is the only successful result
 * When enabled == false:
 *  0 indicates shutdown,
 *  negative indicates an error, though currently nothing can cause it
 *  positive is the current reference count. Means that somebody else is
 *          still using the sensor.
 */
int SensorIIODev::startStop(int enabled)
{
    int alive = mRefCount > 0;
    mRefCount = max(0, enabled ? mRefCount+1 : mRefCount-1);
    int alivenew = mRefCount > 0;
    if (alive == alivenew)
        return enabled ? 0 : mRefCount;

    int ret =0;
    double sensitivity;

    ALOGD(">>%s enabled: %d", device_name.c_str(), enabled);

    if (enabled){
        if ((ret = discover()) < 0) {
            ALOGE("discover failed: %s\n", device_name.c_str());
            return ret;
        }
        if ((ret = open()) < 0) {
            ALOGE("open failed: %s\n", device_name.c_str());
            return ret;
        }

        if (ReadHIDScaleValue(&scale) < 0)
            goto err_ret;
        if (ReadHIDOffsetValue(&offset) < 0)
            goto err_ret;
//        sensitivity =  DeviceGetSensitivity(GetDeviceNumber());
//        if (DeviceSetSensitivity(GetDeviceNumber(), sensitivity) < 0)
//            goto err_ret;
        if (AllocateRxBuffer() < 0)
            goto err_ret;
        if (SetDataReadyTrigger(GetDeviceNumber(), true) < 0)
            goto err_ret;
        if (EnableBuffer(1) < 0)
            goto err_ret;
        if (DeviceActivate(GetDeviceNumber(), 1) < 0)
            goto err_ret;
        ALOGI("%s: scale=%f offset=%f", device_name.c_str(), scale, offset);
    }
    else
    {
        // when going down, ignore all errors. Cleanup means cleanup!
        DeviceActivate(GetDeviceNumber(), 0);
        EnableBuffer(0);
        SetDataReadyTrigger(GetDeviceNumber(), false);
        FreeRxBuffer();
        mDevPath = "";
        close();
    }
    return 0;

err_ret:
    close();
    ALOGE("SesnorIIO: %s Enable failed", device_name.c_str());
    return -1;
}

int SensorIIODev::setDelay(int64_t delay_ns){
    int ms = nsToMs(delay_ns);
    int r;

    ALOGV(">>%s %ld", __func__, delay_ns);
    if (IsDeviceInitialized() == false){
        ALOGE("Device %s was not initialized", device_name.c_str());
        return  -EFAULT;
    }
    if (ms){
        if ((r = SetSampleDelay(GetDeviceNumber(), ms)) < 0)
            return r;
    }
    ALOGV("<<%s", __func__);
    return 0;
}

int SensorIIODev::setInitialState(){
    mHasPendingEvent = false;
    return 0;
}

float SensorIIODev::GetScaleValue()
{
    return scale;
}

float SensorIIODev::GetOffsetValue()
{
    return offset;
}

bool SensorIIODev::IsDeviceInitialized(){
    return initialized;
}

int SensorIIODev::GetDeviceNumber(){
    return device_number;
}

int SensorIIODev::GetDir(const std::string& dir, std::vector < std::string >& files){
    DIR *dp;
    struct dirent *dirp;

    if ((dp = opendir(dir.c_str())) == NULL){
        ALOGE("Error opening directory %s\n", (char*)dir.c_str());
        return errno;
    }
    while ((dirp = readdir(dp)) != NULL){
        files.push_back(std::string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

void SensorIIODev::ListFiles(const std::string& dir){
    std::vector < std::string > files = std::vector < std::string > ();

    GetDir(dir, files);

    for (unsigned int i = 0; i < files.size(); i++){
        ALOGV("File List.. %s\n", (char*)files[i].c_str());
    }
}

// This function returns a device number for IIO device
// For example if the device is "iio:device1", this will return 1
int SensorIIODev::FindDeviceNumberFromName(const std::string& name, const std::string&
    prefix){
    int dev_number;
    std::vector < std::string > files = std::vector < std::string > ();
    std::string device_name;

    GetDir(std::string(IIO_DIR), files);

    for (unsigned int i = 0; i < files.size(); i++){
        std::stringstream ss;
        if ((files[i] == ".") || (files[i] == "..") || files[i].length() <=
            prefix.length())
            continue;

        if (files[i].compare(0, prefix.length(), prefix) == 0){
            sscanf(files[i].c_str() + prefix.length(), "%d", &dev_number);
        }
        ss << IIO_DIR << "/" << prefix << dev_number;
        std::ifstream ifs(ss.str().c_str(), std::ifstream::in);
        if (!ifs.good()){
            dev_number =  -1;
            ifs.close();
            continue;
        }
        ifs.close();

        std::ifstream ifn((ss.str() + "/" + "name").c_str(), std::ifstream::in);

        if (!ifn.good()){
            dev_number =  -1;
            ifn.close();
            continue;
        }
        std::getline(ifn, device_name);
        if (name.compare(device_name) == 0){
            ALOGV("matched %s\n", device_name.c_str());
            ifn.close();
            return dev_number;
        }
        ifn.close();

    }
    return  -EFAULT;
}

// Setup Trigger
int SensorIIODev::SetUpTrigger(int dev_num){
    std::stringstream trigger_name;
    int trigger_num;

    trigger_name << device_name << "-dev" << dev_num;
    if (trigger_name.str().length()){
        ALOGV("trigger_name %s\n", (char*)trigger_name.str().c_str());
        trigger_num = FindDeviceNumberFromName(trigger_name.str(), "trigger");
        if (trigger_num < 0){
            ALOGE("Failed to find trigger\n");
            return  -EFAULT;
        }
    }
    else{
        ALOGE("trigger_name not found \n");
        return  -EFAULT;
    }
    return 0;
}

// Setup buffer length in terms of datum size
int SensorIIODev::SetUpBufferLen(int len){
    std::stringstream filename;
    std::stringstream len_str;

    ALOGV("%s: len:%d", __func__, len);

    filename << buffer_dir_name.str() << "/" << "length";

    PathOps path_ops;
    len_str << len;
    int ret = path_ops.write(filename.str(), len_str.str());
    if (ret < 0) {
        ALOGE("Write Error %s", filename.str().c_str());
        return ret;
    }
    buffer_len = len;
    return 0;
}

// Enable buffer
int SensorIIODev::EnableBuffer(int status){
    std::stringstream filename;
    std::stringstream status_str;

    ALOGV(">>%s:%s: status:%d", device_name.c_str(), __func__, status);

    filename << buffer_dir_name.str() << "/" << "enable";
    PathOps path_ops;
    status_str << status;
    int ret = path_ops.write(filename.str(), status_str.str());
    if (ret < 0) {
        ALOGE("Write Error %s", filename.str().c_str());
        return ret;
    }
    enable_buffer = status;
    ALOGV("<<%s:%s: status:%d", device_name.c_str(), __func__, status);
    return 0;
}

int SensorIIODev::EnableChannels(){
    int ret = 0;
    int count;
    int counter = 0;
    std::string dir = std::string(IIO_DIR);
    std::vector < std::string > files = std::vector < std::string > ();
    const std::string FORMAT_SCAN_ELEMENTS_DIR = "/scan_elements";
    unsigned char signchar, bits_used, total_bits, shift, unused;
    SensorIIOChannel iio_channel;

    ALOGV(">>%s:%s: dev_device_name:%s", device_name.c_str(), __func__, dev_device_name.str().c_str());
    scan_el_dir.str(std::string());
    scan_el_dir << dev_device_name.str() << FORMAT_SCAN_ELEMENTS_DIR;
    GetDir(scan_el_dir.str(), files);

    // If buffer is enabled channels cannot be changed. Disable buffer in case
    // it has been enabled by some previous app
    EnableBuffer(0);
    for (unsigned int i = 0; i < files.size(); i++){
        int len = files[i].length();
        if (len > 3 && files[i].compare(len - 3, 3, "_en") == 0){
            std::stringstream filename, file_type;
            PathOps path_ops;

            filename << scan_el_dir.str() << "/" << files[i];
            ret = path_ops.write(filename.str(), "1");
            if (ret < 0)
              ALOGE("Channel Enable Error %s:%d", filename.str().c_str(), ret);
        }
    }
    ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, ret);
    return ret;
}

int SensorIIODev::BuildChannelList(){
    int ret;
    int count;
    int counter = 0;
    std::string dir = std::string(IIO_DIR);
    std::vector < std::string > files = std::vector < std::string > ();
    const std::string FORMAT_SCAN_ELEMENTS_DIR = "/scan_elements";
    unsigned char signchar, bits_used, total_bits, shift, unused;
    SensorIIOChannel iio_channel;
    std::string type_name;

    ALOGV(">>%s:%s: dev_device_name:%s", device_name.c_str(), __func__, dev_device_name.str().c_str());
    scan_el_dir.str(std::string());
    scan_el_dir << dev_device_name.str() << FORMAT_SCAN_ELEMENTS_DIR;
    GetDir(scan_el_dir.str(), files);

    // File ending with _en will specify a channel
    // it contains the count. If we add all these count
    // those many channels present
    info_array.clear();
    for (unsigned int i = 0; i < files.size(); i++){
        int len = files[i].length();
        // At the least the length should be more than 3 for "_en"
        if (len > 3 && files[i].compare(len - 3, 3, "_en") == 0){
            std::stringstream filename, file_type;
            filename << scan_el_dir.str() << "/" << files[i];
            std::ifstream ifs(filename.str().c_str(), std::ifstream::in);

            if (!ifs.good()){
                ifs.close();
                continue;
            }
            count = ifs.get() - '0';
            if (count == 1)
                counter++;
            ifs.close();

            iio_channel.enabled = 1;

            iio_channel.name = files[i].substr(0, files[i].length() - 3);

            ALOGV("IIO channel name:%s\n", (char*)iio_channel.name.c_str());
            file_type << scan_el_dir.str() << "/" << iio_channel.name <<
                "_type";

            std::ifstream its(file_type.str().c_str(), std::ifstream::in);
            if (!its.good()){
                its.close();
                continue;
            }
            std::getline(its, type_name);
            sscanf(type_name.c_str(), "%c%c%c%c%u/%u>>%u", (unsigned char*) &unused,
                (unsigned char*) &unused, (unsigned char*) &unused, (unsigned
                char*) &signchar, (unsigned int*) &bits_used, (unsigned int*)
                &total_bits, (unsigned int*) &shift);

            its.close();
            // Buggy sscanf on some platforms
            if (total_bits == 0 || bits_used == 0){
                if (!strcmp(type_name.c_str(), "le:s16/32")){
                    total_bits = 32;
                    bits_used = 16;
                    signchar = 's';
                }
                else if (!strcmp(type_name.c_str(), "le:s16/16")){
                    total_bits = 16;
                    bits_used = 16;
                    signchar = 's';
                }
                else if (!strcmp(type_name.c_str(), "le:s32/32")){
                    total_bits = 32;
                    bits_used = 32;
                    signchar = 's';
                }
                else if (!strcmp(type_name.c_str(), "le:s32/32X4>>0"))
                {
                    total_bits = 32*4;
                    bits_used = 32*4;
                    signchar = 's';
                }
                else{
                    total_bits = 32;
                    bits_used = 16;
                }
            }

            iio_channel.bytes = total_bits / 8;
            iio_channel.real_bytes = bits_used / 8;
            iio_channel.shift = shift;
            iio_channel.is_signed = signchar;

            // Add to list
            info_array.push_back(iio_channel);
        }
    }
    ALOGV("<<%s:%s: counter:%d", device_name.c_str(), __func__, counter);
    return counter;
}

int SensorIIODev::GetSizeFromChannels(){
    int size = 0;
    for (unsigned int i = 0; i < info_array.size(); i++){
        SensorIIOChannel channel = info_array[i];
        size += channel.bytes;
    }
    ALOGD("%s:%d:%d", __func__, info_array.size(), size);
    return size;
}

int SensorIIODev::GetChannelBytesUsedSize(unsigned int channel_no){
    if (channel_no < info_array.size()){
        SensorIIOChannel channel = info_array[channel_no];
        return channel.real_bytes;
    }
    else
        return 0;
}

int SensorIIODev::ParseIIODirectory(const std::string& name){
    int dev_num;
    int ret;
    int size;

    ALOGV(">>%s:%s: name:%s", device_name.c_str(), __func__, name.c_str());

    dev_device_name.str(std::string());
    buffer_dir_name.str(std::string());

    device_number = dev_num = FindDeviceNumberFromName(name, "iio:device");
    if (dev_num < 0){
        ALOGE("Failed to find device %s", (char*)name.c_str());
        return  -EFAULT;
    }

    // Construct device directory Eg. /sys/devices/iio:device0
    dev_device_name << IIO_DIR << "/" << "iio:device" << dev_num;

    // Construct device directory Eg. /sys/devices/iio:device0:buffer0
    buffer_dir_name << dev_device_name.str() << "/" << "buffer";

    // Setup Trigger
    ret = SetUpTrigger(dev_num);
    if (ret < 0){
        ALOGE("ParseIIODirectory Failed due to Trigger\n");
        return  -EFAULT;
    }

    // Setup buffer len. This is in the datum units, not an absolute number
    SetUpBufferLen(DEF_BUFFER_LEN);

    // Set up channel masks
    ret = EnableChannels();
    if (ret < 0){
        ALOGE("ParseIIODirectory Failed due Enable Channels failed\n");
	if (ret == -EACCES) {
            // EACCES means the nodes do not have current owner.
            // Need to retry, or else sensors won't power on.
            // Other errors can be ignored, as these nodes are
            // set once, and will return error when set again.
            return ret;
        }
    }

    // Parse the channels and build a list
    ret = BuildChannelList();
    if (ret < 0){
        ALOGE("ParseIIODirectory Failed due BuildChannelList\n");
        return  -EFAULT;
    }

    datum_size = GetSizeFromChannels();
    ALOGV("<<%s:%s: datum_size:%d", device_name.c_str(), __func__, datum_size);
    return 0;
}

// Set Data Ready
int SensorIIODev::SetDataReadyTrigger(int dev_num, bool status){
    std::stringstream filename;
    std::stringstream trigger_name;
    int trigger_num;

    ALOGV(">>%s:%s: status:%d", device_name.c_str(), __func__, status);

    filename << dev_device_name.str() << "/trigger/current_trigger";
    trigger_name << device_name << "-dev" << dev_num;

    PathOps path_ops;
    int ret;
    if (status)
        ret = path_ops.write(filename.str(), trigger_name.str());
    else
        ret = path_ops.write(filename.str(), " ");
    if (ret < 0) {
        ALOGE("Write Error %s:%d", filename.str().c_str(), ret);
        // Ignore error, as this may be due to
        // Trigger was active during resume
    }
    ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, 0);
    return 0;
}

// Activate device
int SensorIIODev::DeviceActivate(int dev_num, int state){
    std::stringstream filename;
    std::stringstream activate_str;

    ALOGV("%s:%s: state:%d", device_name.c_str(), __func__, state);
    return 0;
}

// Get sensitivity in absolute terms
double SensorIIODev::DeviceGetSensitivity(int dev_num){
    std::stringstream filename;
    std::string sensitivity_str;
    double value;

    filename << IIO_DIR << "/" << "iio:device" << dev_num << "/" << channel_prefix_str << "hysteresis";

    PathOps path_ops;
    int ret = path_ops.read(filename.str(), sensitivity_str);
    if (ret < 0) {
        ALOGE("Read Error %s", filename.str().c_str());
	return DEF_HYST_VALUE;
    }
    istringstream buffer(sensitivity_str);
    buffer >> value;

    ALOGV("%s:%s: value:%f", device_name.c_str(), __func__, value);
    return value;
}

// Set sensitivity in absolute terms
int SensorIIODev::DeviceSetSensitivity(int dev_num, double value){
    std::stringstream filename;
    std::stringstream sensitivity_str;

    filename << IIO_DIR << "/" << "iio:device" << dev_num << "/" << channel_prefix_str << "hysteresis";

    PathOps path_ops;
    sensitivity_str << value;
    int ret = path_ops.write(filename.str(), sensitivity_str.str());
    if (ret < 0) {
        ALOGE("Write Error %s", filename.str().c_str());
        // Don't bail out as this  field may be absent
    }
    ALOGV("%s:%s: value:%f", device_name.c_str(), __func__, value);
    return 0;
}

// Set the sample period delay: units ms
int SensorIIODev::SetSampleDelay(int dev_num, int period){
    std::stringstream filename;
    std::stringstream sample_rate_str;

    if (sample_delay_min_ms && period < sample_delay_min_ms)
        period = sample_delay_min_ms;

    filename << IIO_DIR << "/" << "iio:device" << dev_num << "/" << channel_prefix_str << "sampling_frequency";
    if (period <= 0) {
        ALOGE("%s: Invalid_rate:%d", __func__, period);
    }
    PathOps path_ops;
    sample_rate_str << 1000/period;
    int ret = path_ops.write(filename.str(), sample_rate_str.str());
    if (ret < 0) {
        ALOGE("Write Error %s", filename.str().c_str());
        // Don't bail out as this  field may be absent
    }
    ALOGV("%s:%s: period:%d", device_name.c_str(), __func__, period);
    return 0;
}

int SensorIIODev::readEvents(sensors_event_t *data, int count){
    ssize_t read_size;
    int numEventReceived;

    ALOGV(">>%s:%s: count:%d", device_name.c_str(), __func__, count);

    if (count < 1)
    {
        ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, -EINVAL);
        return  - EINVAL;
    }

    if (mFd < 0)
    {
        ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, -EBADF);
        return  - EBADF;
    }

    if (!raw_buffer)
    {
        ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, -EAGAIN);
        return - EAGAIN;
    }

    if (mHasPendingEvent){
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        ALOGV("<<%s:%s: return:%d", device_name.c_str(), __func__, 1);
        return 1;
    }
    read_size = read(mFd, raw_buffer, datum_size);
    numEventReceived = 0;
    if(read_size != datum_size) {
        ALOGE("read() error (or short count) from IIO device: %d\n", errno);
    } else {
        // Always call processEvent(), but only emit output if
        // enabled.  We may have slave devices (e.g. we could be a
        // RotVec serving a SynthCompass without clients of our own).
        if (processEvent(raw_buffer, datum_size) >= 0) {
            if (mEnabled) {
                mPendingEvent.timestamp = getTimestamp();
                *data = mPendingEvent;
                numEventReceived++;
            }
        }
    }
    ALOGV("<<%s:%s: numEventReceived:%d", device_name.c_str(), __func__, numEventReceived);
    return numEventReceived;
}

static int ReadHIDSysValue(int dev, const std::string& sys_str, float *value){
    std::stringstream filename;
    int size;
    std::string long_str;

    filename << IIO_DIR << "/" << "iio:device" << dev  << "/" << sys_str;

    std::ifstream its(filename.str().c_str(), std::ifstream::in);
    if (!its.good()){
        ALOGE("%s: Can't Open :%s",
                __func__, filename.str().c_str());
        its.close();
        return -EINVAL;
    }
    std::getline(its, long_str);
    its.close();

    if (long_str.length() > 0){
        *value = atof(long_str.c_str());
        ALOGV("%s: read %s %f", __func__, filename.str().c_str(), *value);
        return 0;
    }
    ALOGE("%s: read %s failed", __func__, filename.str().c_str());
    return  -EINVAL;
}

int SensorIIODev::ReadHIDScaleValue(float *scale){
    return ReadHIDSysValue(device_number, scale_str, scale);
}

int SensorIIODev::ReadHIDOffsetValue(float *offset){
    return ReadHIDSysValue(device_number, offset_str, offset);
}
