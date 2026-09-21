#include "Camera.h"              // 类声明
#include "MvCameraControl.h"     // 海康 SDK
#include <opencv2/opencv.hpp>    // cv::Mat
#include <cstring>               // memset
#include <iostream>              // std::cout / std::cerr
#include <vector>

// 枚举 USB 相机，返回每台相机的序列号；枚举失败或没有设备时返回空列表。
std::vector<std::string> Camera::listcameras()
	{
		std::vector<std::string> serials;
		MV_CC_DEVICE_INFO_LIST List;
		memset(&List, 0, sizeof(List));
		int nRet = MV_CC_EnumDevicesEx2(MV_USB_DEVICE, &List,nullptr,SortMethod_SerialNumber);
		if (MV_OK != nRet || List.nDeviceNum == 0)
		{
			return serials;
		}
		serials.reserve(List.nDeviceNum);
		for (unsigned int i = 0; i < List.nDeviceNum; ++i)
		{
			const auto& serial = List.pDeviceInfo[i]->SpecialInfo.stUsb3VInfo.chSerialNumber;
			std::size_t length = 0;
			while (length < sizeof(serial) && serial[length] != '\0')
				++length;
			serials.emplace_back(reinterpret_cast<const char*>(serial), length);
		}
		return serials;
	}

	// 按从 1 开始的编号选择 USB 相机，创建句柄并打开设备；失败时返回 false。
	bool Camera::open(int num)
	{
		MV_CC_DEVICE_INFO_LIST List;
		memset(&List, 0, sizeof(List));

		int nRet = MV_CC_EnumDevicesEx2(MV_USB_DEVICE, &List,nullptr,SortMethod_SerialNumber);
		if (MV_OK != nRet || List.nDeviceNum == 0)
		{
			return false;
		}
		if (num< 1 || num >List.nDeviceNum)
		{
			return false;
		}
		nRet = MV_CC_CreateHandle(&handle_, List.pDeviceInfo[num - 1]);
			if (nRet != MV_OK)
			{
				return false;
			}
		nRet = MV_CC_OpenDevice(handle_);
		if (MV_OK != nRet)
		{
			return false;
		}

		is_open_ = true;
		return true;
	}

	// 初始化海康 MVS SDK。
	SDKContext::SDKContext()
	{
		int nRet = MV_CC_Initialize();   // 初始化 SDK
		if (MV_OK != nRet)
		{
			std::cout<<"SDK 初始化失败";
		}
	}

	// 反初始化海康 MVS SDK。
	SDKContext::~SDKContext()
	{
		MV_CC_Finalize();   // 反初始化 SDK
	}

	// 创建相机对象时把句柄和打开状态设为初始值。
	Camera::Camera()
		: handle_(nullptr)     // 初始化 handle_ 为空
		, is_open_(false)      // 初始化 is_open_ 为 false
	{
	}

	// 销毁对象时停止取流、关闭设备、销毁句柄。
	Camera::~Camera()
	{
		stop();   // 停止拍照（如果还在拍）
		if (handle_)   //启动成功
		{
			MV_CC_CloseDevice(handle_);    // 关闭设备
			MV_CC_DestroyHandle(handle_);  // 销毁句柄
			handle_ = nullptr;             // 句柄置空
		}
	}

	// 将相机设为连续采集模式，注册图像回调并开始取流；失败时返回 false。
	bool Camera::start()
	{
		// 设置触发模式为连续采集（枚举节点用 SetEnumValue）
		int nRet = MV_CC_SetEnumValue(handle_, "TriggerMode", MV_TRIGGER_MODE_OFF);
		if (MV_OK != nRet)
		{
			return false;
		}

		// 注册图像回调；SDK 收到一帧后会调用 imageCallback
		nRet = MV_CC_RegisterImageCallBackEx2(handle_, Camera::imageCallback, this, true);
		if (MV_OK != nRet)
		{
			return false;
		}

		// 开始取流
		nRet = MV_CC_StartGrabbing(handle_);
		if (MV_OK != nRet)
		{
			return false;
		}

		return true;
	}

	// 停止设备取流；SDK 随即停止触发图像回调。
	void Camera::stop()
	{
		if (handle_)
		{
			MV_CC_StopGrabbing(handle_);
		}
	}

	// 将回调缓存的最新一帧深拷贝到 out；尚无可用帧时返回 false。
	bool Camera::getimg(cv::Mat& out)
	{
		// 加锁，保护缓存
		std::lock_guard<std::mutex> guard(frame_mutex_);

		// 缓存为空，说明还没抓到帧
		if (latest_frame_.empty())
		{
			return false;
		}

		// 深拷贝一份给调用方
		out = latest_frame_.clone();
		return true;
	}

	// 查询当前曝光时间（微秒）。成功后写入 exposure，失败时写入 SDK 错误码。
	bool Camera::getExposure(float& exposure, int& errorCode)
	{
		if (!handle_)
		{
			errorCode = MV_E_HANDLE;
			return false;
		}

		MVCC_FLOATVALUE stFloatValue = {0};
		int nRet = MV_CC_GetFloatValue(handle_, "ExposureTime", &stFloatValue);
		if (MV_OK != nRet)
		{
			errorCode = nRet;
			return false;
		}

		exposure = stFloatValue.fCurValue;
		errorCode = MV_OK;
		return true;
	}

	// 关闭自动曝光并设置曝光时间（微秒）；失败时返回 false 并写入错误码。
	bool Camera::setExposure(float exposure, int& errorCode)
	{
		if (!handle_)
		{
			errorCode = MV_E_HANDLE;
			return false;
		}

		// 先关自动曝光，否则 ExposureTime 是只读的
		int nRet = MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
		if (MV_OK != nRet)
		{
			errorCode = nRet;
			return false;
		}

		nRet = MV_CC_SetFloatValue(handle_, "ExposureTime", exposure);
		if (MV_OK != nRet)
		{
			errorCode = nRet;
			return false;
		}

		errorCode = MV_OK;
		return true;
	}

	// 按增量调整曝光时间（微秒）并读回实际值。
	// 相机可能把数值取整到最近的支持档位，所以写入后必须再读一次才是真实生效值。
	bool Camera::changeExposure(float delta, float& actual, int& errorCode)
	{
		if (!handle_)
		{
			errorCode = MV_E_HANDLE;
			return false;
		}

		// 读当前值（同时拿到相机的上下限）
		MVCC_FLOATVALUE stFloatValue = {0};
		int nRet = MV_CC_GetFloatValue(handle_, "ExposureTime", &stFloatValue);
		if (MV_OK != nRet)
		{
			errorCode = nRet;
			return false;
		}

		// 加上增量并夹到相机支持范围内，避免超限写失败
		float target = stFloatValue.fCurValue + delta;
		if (target < stFloatValue.fMin)
			target = stFloatValue.fMin;
		if (target > stFloatValue.fMax)
			target = stFloatValue.fMax;

		// 写回（setExposure 负责关自动曝光）
		if (!setExposure(target, errorCode))
			return false;

		// 读回真实生效值——这是 changeExposure 与 setExposure 的区别
		return getExposure(actual, errorCode);
	}

	// SDK 图像回调：将收到的图像转换为 OpenCV 可用格式并更新最新帧缓存。
	void __stdcall Camera::imageCallback(MV_FRAME_OUT* frameData, void* user, bool autoFree)
	{
		(void)autoFree;
		Camera* camera = static_cast<Camera*>(user);
		if (camera == nullptr || frameData == nullptr || frameData->pBufAddr == nullptr)
		{
			return;
		}

		// 取帧信息（宽、高、像素格式）
		const MV_FRAME_OUT_INFO_EX& stInfo = frameData->stFrameInfo;

		// 转成 cv::Mat
		cv::Mat frame;
		if (stInfo.enPixelType == PixelType_Gvsp_Mono8)
		{
			frame = cv::Mat(stInfo.nHeight, stInfo.nWidth, CV_8UC1, frameData->pBufAddr);
		}
		else if (stInfo.enPixelType == PixelType_Gvsp_BGR8_Packed)
		{
			frame = cv::Mat(stInfo.nHeight, stInfo.nWidth, CV_8UC3, frameData->pBufAddr);
		}
		else
		{
			std::vector<unsigned char> bgr(static_cast<size_t>(stInfo.nWidth) * stInfo.nHeight * 3);
			MV_CC_PIXEL_CONVERT_PARAM_EX cvt = {};
			cvt.nWidth = stInfo.nWidth;
			cvt.nHeight = stInfo.nHeight;
			cvt.enSrcPixelType = stInfo.enPixelType;
			cvt.pSrcData = frameData->pBufAddr;
			cvt.nSrcDataLen = stInfo.nFrameLen;
			cvt.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
			cvt.pDstBuffer = bgr.data();
			cvt.nDstBufferSize = bgr.size();
			int cvtRet = MV_CC_ConvertPixelTypeEx(camera->handle_, &cvt);
			if (cvtRet == MV_OK)
			{
				cv::Mat converted(stInfo.nHeight, stInfo.nWidth, CV_8UC3, bgr.data());
				std::lock_guard<std::mutex> guard(camera->frame_mutex_);
				camera->latest_frame_ = converted.clone();
			}
			else
			{
				std::cerr << "像素转换失败，格式: 0x" << std::hex << stInfo.enPixelType
				          << " 错误码: 0x" << cvtRet << std::dec << '\n';
			}
			return;
		}

		// SDK 会在回调返回后回收缓存，因此这里深拷贝到 Camera 自己的缓存中
		{
			std::lock_guard<std::mutex> guard(camera->frame_mutex_);
			camera->latest_frame_ = frame.clone();
		}
	}
