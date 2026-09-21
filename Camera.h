// ============================================================
// Camera.h —— 相机类声明（设计图）
// 作用：告诉编译器 Camera 类"有什么"，不写"怎么做"
// ============================================================

#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>   // cv::Mat（存图像）
#include <mutex>                // std::mutex（锁）
#include <string>               // std::string
#include <vector>               // std::vector（相机序列号列表）

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
    friend struct CameraCallbackAdapter; // 允许 camera.cpp 内的 SDK 回调更新帧缓存

    // ===== 成员变量 =====
    void* handle_ = nullptr;       // 相机句柄（初始为空）
    bool    is_open_ = false;        // 是否打开成功（初始未打开）
    std::mutex        frame_mutex_;        // 保护最新帧的锁
    cv::Mat           latest_frame_;       // 最新一帧缓存
};

#endif // CAMERA_H
