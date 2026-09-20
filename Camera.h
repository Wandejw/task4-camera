// ============================================================
// Camera.h —— 相机类声明（设计图）
// 作用：告诉编译器 Camera 类"有什么"，不写"怎么做"
// ============================================================

#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>   // cv::Mat（存图像）
#include <atomic>               // std::atomic<bool>（线程安全标志）
#include <mutex>                // std::mutex（锁）
#include <thread>               // std::thread（线程）
#include <string>               // std::string
#include <vector>               // std::vector（相机序列号列表）

class SDKContext
{
public:
    SDKContext();                 // 初始化 SDK
    ~SDKContext();                // 反初始化 SDK

    SDKContext(const SDKContext&) = delete;
    SDKContext& operator=(const SDKContext&) = delete;
};

class Camera
{
public:
    // ===== 对外接口（main 会调用）=====
    Camera();                    // 构造函数
    ~Camera();                   // 析构函数：关闭相机

    bool open(int number);       // 打开第 number 台相机
    bool start();                // 开始拍照
    bool getimg(cv::Mat& out);   // 抓一帧
    void stop();                 // 停止拍照
    bool getExposure(float& exposure, int& errorCode); // 查询曝光时间（微秒）
    bool setExposure(float exposure, int& errorCode);  // 关闭自动曝光并设置曝光时间（微秒）
    bool changeExposure(float delta, float& actual, int& errorCode); // 按增量调整并读回实际值
    std::vector<std::string> listcameras(); // 返回相机序列号，显示交给 main

private:
    void grabLoop();             // 后台抓图线程函数

    // ===== 成员变量 =====
    void* handle_ = nullptr;       // 相机句柄（初始为空）
    bool    is_open_ = false;        // 是否打开成功（初始未打开）
    std::thread       background_thread_;  // 后台抓图线程
    std::mutex        frame_mutex_;        // 保护最新帧的锁
    cv::Mat           latest_frame_;       // 最新一帧缓存
    std::atomic<bool> should_stop_{true};  // 停止标志（初始为停止状态）
};

#endif // CAMERA_H
