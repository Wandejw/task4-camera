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
		// 【练习 1】让菜单按序列号排序：把下一行换成支持排序的 MV_CC_EnumDevicesEx2。
		// 提示：保留 MV_USB_DEVICE、&List；第三个参数填 nullptr，最后填 SortMethod_SerialNumber。
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

		// 【练习 2】打开相机前还会重新枚举：这里也改成与练习 1 完全相同的排序调用。
		// 检查点：菜单里的第 N 台应对应下方的 List.pDeviceInfo[N - 1]。
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

	// 销毁对象时停止取流线程、关闭设备、销毁句柄。
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

	// 将相机设为连续采集模式，开始取流并启动后台抓图线程；失败时返回 false。
	bool Camera::start()
	{
		// 设置触发模式为连续采集（枚举节点用 SetEnumValue）
		int nRet = MV_CC_SetEnumValue(handle_, "TriggerMode", MV_TRIGGER_MODE_OFF);
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

		// 重置停止标志
		should_stop_ = false;

		// 启动后台抓图线程
		background_thread_ = std::thread(&Camera::grabLoop, this);

		return true;
	}

	// 通知后台线程退出，停止设备取流，并等待线程结束。
	void Camera::stop()
	{
		// 翻停止标志，让后台线程退出
		should_stop_ = true;

		// 先停止取流，让 GetImageBuffer 立即返回（否则线程会阻塞到超时）
		if (handle_)
		{
			MV_CC_StopGrabbing(handle_);
		}

		// 再等后台线程结束
		if (background_thread_.joinable())
		{
			background_thread_.join();
		}
	}

	// 将后台线程缓存的最新一帧深拷贝到 out；尚无可用帧时返回 false。
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

	// 后台循环取帧：将图像转换为 OpenCV 可用格式，更新最新帧缓存并释放 SDK 帧缓冲。
	void Camera::grabLoop()
	{
		while (!should_stop_)
		{
			// 准备帧容器
			MV_FRAME_OUT stFrame;
			memset(&stFrame, 0, sizeof(stFrame));

			// 抓一帧（超时 1000ms）
			int nRet = MV_CC_GetImageBuffer(handle_, &stFrame, 1000);
			if (MV_OK != nRet)
			{
				continue;   // 没抓到，继续下一轮
			}

			// 取帧信息（宽、高、像素格式）
			MV_FRAME_OUT_INFO_EX stInfo;
			memset(&stInfo, 0, sizeof(stInfo));
			stInfo.enPixelType = stFrame.stFrameInfo.enPixelType;
			stInfo.nWidth = stFrame.stFrameInfo.nWidth;
			stInfo.nHeight = stFrame.stFrameInfo.nHeight;

			// 转成 cv::Mat
			cv::Mat frame;
			if (stInfo.enPixelType == PixelType_Gvsp_Mono8)
			{
				frame = cv::Mat(stInfo.nHeight, stInfo.nWidth, CV_8UC1, stFrame.pBufAddr);
			}
			else if (stInfo.enPixelType == PixelType_Gvsp_BGR8_Packed)
			{
				frame = cv::Mat(stInfo.nHeight, stInfo.nWidth, CV_8UC3, stFrame.pBufAddr);
			}
			else
			{
				std::vector<unsigned char> bgr(static_cast<size_t>(stInfo.nWidth) * stInfo.nHeight * 3);
				MV_CC_PIXEL_CONVERT_PARAM_EX cvt = {};
				cvt.nWidth = stInfo.nWidth;
				cvt.nHeight = stInfo.nHeight;
				cvt.enSrcPixelType = stInfo.enPixelType;
				cvt.pSrcData = stFrame.pBufAddr;
				cvt.nSrcDataLen = stFrame.stFrameInfo.nFrameLen;
				cvt.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
				cvt.pDstBuffer = bgr.data();
				cvt.nDstBufferSize = bgr.size();
				int cvtRet = MV_CC_ConvertPixelTypeEx(handle_, &cvt);
				if (cvtRet == MV_OK)
				{
					cv::Mat converted(stInfo.nHeight, stInfo.nWidth, CV_8UC3, bgr.data());
					std::lock_guard<std::mutex> guard(frame_mutex_);
					latest_frame_ = converted.clone();
				}
				else
					std::cerr << "像素转换失败，格式: 0x" << std::hex << stInfo.enPixelType
					          << " 错误码: 0x" << cvtRet << std::dec << '\n';
				MV_CC_FreeImageBuffer(handle_, &stFrame);
				continue;
			}

			// 加锁，存进缓存（深拷贝）
			{
				std::lock_guard<std::mutex> guard(frame_mutex_);
				latest_frame_ = frame.clone();
			}

			// 释放帧缓冲
			MV_CC_FreeImageBuffer(handle_, &stFrame);
		}
	}
