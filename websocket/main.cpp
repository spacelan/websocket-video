#include <stdio.h>
#include <stdlib.h>
#include "libwebsockets.h"
#include <iostream>
#include "pxcsensemanager.h"
#include "pxcimage.h"
#include "pxcsession.h"
#include <windows.h>

#pragma comment(lib, "websockets.lib")

struct RGBA
{
	unsigned char B, G, R, A;
};
unsigned char *buf = nullptr;
int bufSize = 0;
int f = 0;
unsigned int count = 0;
bool received = false;
//PXCSenseManager instance
PXCSenseManager *sm = nullptr;

static int
callback_http(struct libwebsocket_context * that,
struct libwebsocket *wsi,
enum libwebsocket_callback_reasons reason, void *user,
	void *in, size_t len)
{
	return 0;
}

static int
callback_raw_camera_data(struct libwebsocket_context * that,
struct libwebsocket *wsi,
enum libwebsocket_callback_reasons reason,
	void *user, void *in, size_t len)
{
	switch (reason) 
	{
	case LWS_CALLBACK_ESTABLISHED:
	{
		// just log message that someone is connecting
		f = 0;
		count = 0;
		if (buf)
			delete buf;
		buf = nullptr;
		if (sm)
			sm->Release();
		sm = nullptr;
		printf("connection established\n");
		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE: 
	{
		if (f == 0) break;
		SYSTEMTIME time;
		int time_ms;
		unsigned char *p = buf + LWS_SEND_BUFFER_PRE_PADDING;
		if (f > 3){
			GetLocalTime(&time);
			time_ms = time.wMilliseconds + (time.wSecond + (time.wMinute + time.wHour * 60) * 60) * 1000;
			for (int i = 0; i < bufSize; i += 4){
				p[i + (count / 30) % 3] += 10;
				p[i + 3] = 255;
			}
			*((int *)p) = time_ms;
		}
		else{
			int err = sm->AcquireFrame(false);
			if (err < PXC_STATUS_NO_ERROR)
			{
				std::cout << "capture error: " << err << std::endl;
				break;
			}
			GetLocalTime(&time);
			time_ms = time.wMilliseconds + (time.wSecond + (time.wMinute + time.wHour * 60) * 60) * 1000;
			// retrieve the sample
			PXCCapture::Sample *sample = sm->QuerySample();
			// work on the image sample->color
			PXCImage::ImageData data;
			sample->color->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &data);

			unsigned char *q = data.planes[0];
			for (int i = 0; i < bufSize; i += 4)
			{
				p[i] = q[i + 2];
				p[i + 1] = q[i + 1];
				p[i + 2] = q[i];
				p[i + 3] = q[i + 3];
			}
			*((int *)p) = time_ms;
			//memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, data.planes[0], 1228800);

			sample->color->ReleaseAccess(&data);
			// go fetching the next sample
			sm->ReleaseFrame();
		}
		// send response
		// just notice that we have to tell where exactly our response starts. That's
		// why there's `buf[LWS_SEND_BUFFER_PRE_PADDING]` and how long it is.
		// we know that our response has the same length as request because
		// it's the same message in reverse order.
		libwebsocket_write(wsi, p, bufSize, LWS_WRITE_BINARY);
		//std::cout << "send frame " << ++count << ' ' << time_ms << std::endl;
		//std::cout << time.wHour << 'h' << time.wMinute << 'm' << time.wSecond << 's' << time.wMilliseconds << "ms" << std::endl;
		std::cout << time_ms << " -- " << ++count << std::endl;
		//received = false;
		break;
	}
	case LWS_CALLBACK_RECEIVE:
	{
		f = atoi((const char*)in);
		switch (f)
		{
		case 0:
		{
				  if (buf)
					  delete buf;
				  buf = nullptr;
				  if (sm)
					  sm->Release();
				  sm = nullptr;
				  break;
		}
		case 1:
		{
				  if (buf)
					  delete buf;
				  buf = nullptr;
				  if (sm)
					  sm->Release();
				  sm = nullptr;

				  bufSize = 640 * 480 * 4;
				  buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
				  sm = PXCSenseManager::CreateInstance();
				  // Select the color stream
				  sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 640, 480);
				  // Initialize and Stream Samples
				  sm->Init();
				  break;
		}
		case 2:
		{
				  if (buf)
					  delete buf;
				  buf = nullptr;
				  if (sm)
					  sm->Release();
				  sm = nullptr;

				  bufSize = 800 * 600 * 4;
				  buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
				  sm = PXCSenseManager::CreateInstance();
				  // Select the color stream
				  sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 800, 600);
				  // Initialize and Stream Samples
				  sm->Init();
				  break;
		}
		case 3:
		{
				  if (buf)
					  delete buf;
				  buf = nullptr;
				  if (sm){
					  sm->Release();
					  sm;
				  }
				  sm = nullptr;

				  bufSize = 1280 * 960 * 4;
				  buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
				  sm = PXCSenseManager::CreateInstance();
				  // Select the color stream
				  sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 1280, 960);
				  // Initialize and Stream Samples
				  sm->Init();
				  break;
		}
		default:
		{
				   if (buf)
					   delete buf;
				   buf = nullptr;
				   if (sm){
					   sm->Release();
					   sm;
				   }
				   sm = nullptr;

				   bufSize = f * 4;
				   buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
				   break;
		}
		}
		break;
	}
	case LWS_CALLBACK_CLOSED:
	{
		std::cout << "closed" << std::endl;
		f = false;
		count = 0;
		break;
	}
	default:
		break;
	}
	return 0;
}

static struct libwebsocket_protocols protocols[] = {
	/* first protocol must always be HTTP handler */
	{
		"http-only",   // name
		callback_http, // callback
		0              // per_session_data_size
	},
	{
		"raw-camera-data", // protocol name - very important!
		callback_raw_camera_data,   // callback
		0                          // we don't use any per session data
	},
	{
		NULL, NULL, 0   /* End of list */
	}
};

int main(void) {
	struct libwebsocket_context *context;
	// server url will be http://localhost:9000
	// we're not using ssl
	// no special options
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof info);
	info.port = 9000;
	info.gid = -1;
	info.uid = -1;
	info.protocols = protocols;

	// create libwebsocket context representing this server
	context = libwebsocket_create_context(&info);

	if (context == NULL) {
		fprintf(stderr, "libwebsocket init failed\n");
		return -1;
	}

	printf("starting server...\n");

	// infinite loop, to end this server send SIGTERM. (CTRL+C)
	while (1) 
	{
		if (f != 0)
			libwebsocket_callback_on_writable_all_protocol(&protocols[1]);
		libwebsocket_service(context, 10);
	}
	libwebsocket_context_destroy(context);
	system("pause");
	return 0;
}