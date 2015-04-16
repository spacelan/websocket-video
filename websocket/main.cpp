#include "libwebsockets.h"
#include "pxcsensemanager.h"
#include "pxcimage.h"
#include "pxcsession.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "websockets.lib")

class MyDeviceInfo: public PXCCapture::DeviceInfo
{
public:
	std::vector<PXCCapture::Device::StreamProfileSet> profiles;
	int numOfprofile;
};

std::string wcharTochar(wchar_t *);
void getDeviceInfo();

int f = 0;
std::vector<MyDeviceInfo> dinfo;
int numOfDevice = 0;
PXCSession *ss = nullptr;

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
	static unsigned char *buf = nullptr;
	static int bufSize = 0;
	static size_t count = 0;
	static PXCSenseManager *sm = nullptr;

	switch (reason) 
	{
	case LWS_CALLBACK_ESTABLISHED:
	{
		printf("connection established\n");
		f = 0;
		count = 0;
		if (buf)
			delete buf;
		buf = nullptr;
		if (sm)
			sm->Release();
		sm = nullptr;
		sm = ss->CreateSenseManager();

		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE: 
	{
		if (f == 0) break;
		SYSTEMTIME time;
		int time_ms;
		unsigned char *p = buf + LWS_SEND_BUFFER_PRE_PADDING;
		if (f > 1){
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
		if (buf)
			delete buf;
		buf = nullptr;
		sm->Close();
		switch (f)
		{
		case 0:
		{
			break;
		}
		case 1:
		{
			sm->QueryCaptureManager()->FilterByDeviceInfo(&dinfo[numOfDevice]);
			int width = dinfo[numOfDevice].profiles[dinfo[numOfDevice].numOfprofile].color.imageInfo.width;
			int height = dinfo[numOfDevice].profiles[dinfo[numOfDevice].numOfprofile].color.imageInfo.height;
			sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, width, height);
			bufSize = width * height * 4;
			buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
			sm->Init();
			break;
		}
		default:
		{
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
		if (buf)
			delete buf;
		buf = nullptr;
		if (sm)
			sm->Release();
		sm = nullptr;
		break;
	}
	default:
		break;
	}
	return 0;
}

static int
callback_camera_info(struct libwebsocket_context * that,
struct libwebsocket *wsi,
enum libwebsocket_callback_reasons reason,
	void *user, void *in, size_t len)
{
	char printBuf[20];
	unsigned char *buf;
	unsigned char *p;
	size_t bufSize;
	switch (reason)
	{
	case LWS_CALLBACK_ESTABLISHED:
	{
		getDeviceInfo();
		if (dinfo.size()){
			sprintf_s(printBuf, "name");
			std::string nameToSend(printBuf);
			for (size_t i = 0; i < dinfo.size(); i++){
				std::string name = wcharTochar(dinfo[i].name);
				nameToSend += "," + name;
			}
			bufSize = nameToSend.length();
			buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
			p = buf + LWS_SEND_BUFFER_PRE_PADDING;
			memcpy(p, nameToSend.c_str(), bufSize);
			libwebsocket_write(wsi, p, bufSize, LWS_WRITE_TEXT);
			delete buf;
			buf = p = nullptr;
			
			sprintf_s(printBuf, "profile");
			std::string profileToSend(printBuf);
			for (size_t i = 0; i < dinfo[0].profiles.size(); i++){
				sprintf_s(printBuf, ",%dx%d", dinfo[0].profiles[i].color.imageInfo.width, dinfo[0].profiles[i].color.imageInfo.height);
				profileToSend.append(printBuf);
			}
			bufSize = profileToSend.length();
			buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
			p = buf + LWS_SEND_BUFFER_PRE_PADDING;
			memcpy(p, profileToSend.c_str(), bufSize);
			libwebsocket_write(wsi, p, bufSize, LWS_WRITE_TEXT);
			delete buf;
			buf = p = nullptr;
		}
		numOfDevice = 0;
		break;
	}
	case LWS_CALLBACK_SERVER_WRITEABLE:
	{

	}
	case LWS_CALLBACK_RECEIVE:
	{
		int num = atoi((const char*)in);
		if (num < 10){
			numOfDevice = num;
			sprintf_s(printBuf, "profile");
			std::string profileToSend(printBuf);
			for (size_t i = 0; i < dinfo[numOfDevice].profiles.size(); i++){
				sprintf_s(printBuf, ",%dx%d", dinfo[numOfDevice].profiles[i].color.imageInfo.width, dinfo[numOfDevice].profiles[i].color.imageInfo.height);
				profileToSend.append(printBuf);
			}
			bufSize = profileToSend.length();
			buf = new unsigned char[bufSize + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
			p = buf + LWS_SEND_BUFFER_PRE_PADDING;
			memcpy(p, profileToSend.c_str(), bufSize);
			libwebsocket_write(wsi, p, bufSize, LWS_WRITE_TEXT);
			delete buf;
			buf = p = nullptr;
		}
		else{
			dinfo[numOfDevice].numOfprofile = num - 10;
		}
	}
	case LWS_CALLBACK_CLOSED:
	{

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
		"camera-info",
		callback_camera_info,
		0
	},
	{
		NULL, NULL, 0   /* End of list */
	}
};

void getDeviceInfo()
{
	PXCSession::ImplDesc desc1 = {};
	desc1.group = PXCSession::IMPL_GROUP_SENSOR;
	desc1.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;
	PXCSession::ImplDesc desc2;

	dinfo.clear();

	if (ss->QueryImpl(&desc1, 0, &desc2) >= PXC_STATUS_NO_ERROR){
		PXCCapture *capture;
		ss->CreateImpl<PXCCapture>(&desc2, &capture);
		for (int d = 0;; d++) {
			MyDeviceInfo info;
			if (capture->QueryDeviceInfo(d, &info) < PXC_STATUS_NO_ERROR) break;
			PXCCapture::Device *device = capture->CreateDevice(d);
			for (int p = 0;; p++){
				PXCCapture::Device::StreamProfileSet profile = {};
				if (device->QueryStreamProfileSet(PXCCapture::STREAM_TYPE_COLOR, p, &profile) < PXC_STATUS_NO_ERROR) break;
				info.profiles.push_back(profile);
				info.numOfprofile = 0;
			}
			dinfo.push_back(info);
		}
		capture->Release();
	}
}

std::string wcharTochar(wchar_t *w)
{
	size_t len = wcslen(w) + 1;
	size_t converted = 0;
	char *CStr = new char[len];
	wcstombs_s(&converted, CStr, len, w, _TRUNCATE);
	std::string str(CStr);
	delete CStr;
	return str;
}

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

	ss = PXCSession::CreateInstance();
	// infinite loop, to end this server send SIGTERM. (CTRL+C)
	while (1) 
	{
		if (f != 0)
			libwebsocket_callback_on_writable_all_protocol(&protocols[1]);
		libwebsocket_service(context, 10);
	}
	ss->Release();
	libwebsocket_context_destroy(context);
	system("pause");
	return 0;
}
