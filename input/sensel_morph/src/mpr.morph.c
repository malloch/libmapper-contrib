/******************************************************************************************
* MIT License
*
* Copyright (c) 2013-2017 Sensel, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif
#include "sensel.h"
#include "sensel_device.h"
#include "mpr/mpr.h"

static bool enter_pressed = false;

#ifdef WIN32
DWORD WINAPI waitForEnter()
#else
void * waitForEnter()
#endif
{
    getchar();
    enter_pressed = true;
    return 0;
}

int main(int argc, char **argv)
{
	//Handle that references a Sensel device
	SENSEL_HANDLE handle = NULL;
	//List of all available Sensel devices
	SenselDeviceList list;
	//Sensor info from the Sensel device
	SenselSensorInfo sensor_info;
	//SenselFrame data that will hold the forces
    SenselFrameData *frame = NULL;

    unsigned int last_n_contacts = 0;

	//Get a list of available Sensel devices
	senselGetDeviceList(&list);
	if (list.num_devices == 0)
	{
		fprintf(stdout, "No device found\n");
		fprintf(stdout, "Press Enter to exit example\n");
		getchar();
		return 0;
	}

    // create libmapper device
    mpr_dev dev = mpr_dev_new("morph", NULL);
    while (!mpr_dev_get_is_ready(dev)) {
        mpr_dev_poll(dev, 25);
    }

	//Open a Sensel device by the id in the SenselDeviceList, handle initialized 
	senselOpenDeviceByID(&handle, list.devices[0].idx);
	//Get the sensor info
	senselGetSensorInfo(handle, &sensor_info);
	
	//Set the frame content to scan force data
	senselSetFrameContent(handle, FRAME_CONTENT_CONTACTS_MASK | FRAME_CONTENT_ACCEL_MASK);
	//Allocate a frame of data, must be done before reading frame data
	senselAllocateFrameData(handle, &frame);
	//Start scanning the Sensel device
	senselStartScanning(handle);

    // force the serial connection to remain open
    // https://forum.sensel.com/t/pausing-e-g-in-a-debugger-causes-all-subsequent-apis-to-fail/219/15
    unsigned char val[1] = { 255 };
    senselWriteReg(handle, 0xD0, 1, val);
    
    fprintf(stdout, "Press Enter to exit example\n");
    #ifdef WIN32
        HANDLE thread = CreateThread(NULL, 0, waitForEnter, NULL, 0, NULL);
    #else
        pthread_t thread;
        pthread_create(&thread, NULL, waitForEnter, NULL);
    #endif
    
    // add signals
//    int mini = 0, maxi = 8192;
//    mpr_sig raw_force = mpr_sig_new(dev, MPR_DIR_OUT, "raw/force", 19425,
//                                    MPR_INT32, "grams", NULL, NULL, NULL, NULL,
//                                    0);
    int mini[2] = {0, 0}, maxi[2] = {16, 16};
    mpr_sig num_contacts = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/num_contacts",
                                       1, MPR_INT32, NULL, mini, maxi, NULL, NULL, 0);
    
    // TODO check these ranges!
    float minf = 0.f, maxf = 1.0f;
    mpr_sig acceleration = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/acceleration",
                                       3, MPR_INT32, "G", &minf, &maxf, NULL, NULL, 0);

    int num_inst = 16;
    maxi[0] = 240;
    maxi[1] = 139;
    mpr_sig position = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/position",
                                   2, MPR_FLT, "mm", mini, maxi, &num_inst,
                                   NULL, 0);
    maxi[0] = 33360;
    mpr_sig area = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/area",
                               1, MPR_FLT, "mm^2", mini, maxi, &num_inst,
                               NULL, 0);
    maxi[0] = 8192;
    mpr_sig force = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/force",
                                1, MPR_FLT, NULL, mini, maxi, &num_inst,
                                NULL, 0);
    maxi[0] = 360;
    mpr_sig orientation = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/orientation",
                                      1, MPR_FLT, "degrees", mini, maxi, &num_inst,
                                      NULL, 0);
    maxi[0] = 240;
    maxi[1] = 139;
    mpr_sig axes = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/axes",
                               2, MPR_FLT, "mm", mini, maxi, &num_inst,
                               NULL, 0);
    
    mpr_sig velocity = mpr_sig_new(dev, MPR_DIR_OUT, "instrument/contact/velocity",
                                   2, MPR_FLT, "mm/sec", NULL, NULL, &num_inst,
                                   NULL, 0);

    mpr_time time;
    
    while (!enter_pressed) {
		unsigned int num_frames = 0;
		//Read all available data from the Sensel device
		senselReadSensor(handle);
		//Get number of frames available in the data read from the sensor
		senselGetNumAvailableFrames(handle, &num_frames);
		for (int f = 0; f < num_frames; f++) {
			//Read one frame of data
			senselGetFrame(handle, frame);

            mpr_time_set(&time, MPR_NOW);
            mpr_dev_start_queue(dev, time);

            if (!frame->n_contacts && !last_n_contacts)
                continue;
            else if (frame->n_contacts != last_n_contacts)
                printf("num_contacts: %d\n", frame->n_contacts);

            mpr_sig_set_value(num_contacts, 0, 1, MPR_INT32, &frame->n_contacts, time);
            mpr_sig_set_value(acceleration, 0, 3, MPR_INT32, &frame->accel_data, time);
            
            for (int c = 0; c < frame->n_contacts; c++) {
                SenselContact sc = frame->contacts[c];
                unsigned int state = sc.state;
                register int id = (int)sc.id;

                switch (state) {
                    case CONTACT_START:
                    case CONTACT_MOVE:
                        mpr_sig_set_value(position, id, 2, MPR_FLT, &sc.x_pos, time);
                        mpr_sig_set_value(velocity, id, 2, MPR_FLT, &sc.delta_x, time);
                        mpr_sig_set_value(orientation, id, 1, MPR_FLT, &sc.orientation, time);
                        mpr_sig_set_value(axes, id, 2, MPR_FLT, &sc.major_axis, time);
                        mpr_sig_set_value(force, id, 1, MPR_FLT, &sc.total_force, time);
                        mpr_sig_set_value(area, id, 1, MPR_FLT, &sc.area, time);
                        break;
                    default:
                        mpr_sig_release_inst(position, id, time);
                        mpr_sig_release_inst(velocity, id, time);
                        mpr_sig_release_inst(orientation, id, time);
                        mpr_sig_release_inst(axes, id, time);
                        mpr_sig_release_inst(force, id, time);
                        mpr_sig_release_inst(area, id, time);
                        break;
                }
            }
            mpr_dev_send_queue(dev, time);
            last_n_contacts = frame->n_contacts;
		}
        mpr_dev_poll(dev, 0);
	}

    // allow serial connection to close
    val[0] = 0;
    senselWriteReg(handle, 0xD0, 1, val);

    mpr_dev_free(dev);
	return 0;
}
