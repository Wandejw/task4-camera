// 上层交互：选择相机、接收按键、显示画面
#include "Camera.h"
#include "SDKContext.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <termios.h>
#include <unistd.h>

void handleExposureKey(Camera& cam, int input)
{
    if (input != 'e' && input != '=' && input != '-')
        return;

    float exposure = 0.0f;
    int errorCode = 0;
    if (input == 'e')
    {
        if (cam.getExposure(exposure, errorCode))
            std::cout << "当前曝光时间: " << exposure << " 微秒" << std::endl;
        else
            std::cout << "查询曝光失败，错误码: 0x" << std::hex
                      << static_cast<unsigned int>(errorCode) << std::dec << std::endl;
        return;
    }

    if (cam.changeExposure(input == '=' ? 500.0f : -500.0f, exposure, errorCode))
        std::cout << "相机实际曝光时间: " << exposure << " 微秒" << std::endl;
    else
        std::cout << "修改曝光或读取实际值失败，错误码: 0x" << std::hex
                  << static_cast<unsigned int>(errorCode) << std::dec << std::endl;
}

int main()
{
    SDKContext sdk;                // 初始化 SDK，退出时反初始化
    Camera cam;                    // 创建相机

    auto serials = cam.listcameras();
    if (serials.empty())
    {
        std::cout << "没有相机";
        return 1;
    }
    for (std::size_t i = 0; i < serials.size(); ++i)
        std::cout << (i + 1) << ". " << serials[i] << "\n";
    int number = 0;
    std::cout << "请输入相机编号并按回车：" << std::flush;
    std::cin >> number;             // 用户选择
    if (!cam.open(number))          // 打开所选相机
    {
        std::cout << "创建失败";
        return 1;
    }

    cv::namedWindow("Camera", cv::WINDOW_NORMAL);
    termios yuan{};
    termios xian{};
    bool if_zhongduan = isatty(STDIN_FILENO);
    if (if_zhongduan)
    {
        int yzcg = tcgetattr(STDIN_FILENO, &yuan);
        if_zhongduan = (yzcg == 0);
    }
    if (if_zhongduan)
    {
        xian = yuan;
        xian.c_lflag &= ~ICANON;
        xian.c_cc[VMIN] = 0;
        xian.c_cc[VTIME] = 0;
        int apply = tcsetattr(STDIN_FILENO, TCSANOW, &xian);
        if_zhongduan = (apply == 0);
    }

    std::cout << "按 a 开始抓帧；按 e 查询曝光；按 = 增加、- 减少曝光（每次 500 微秒）；按 Esc 退出程序。" << std::endl;
    while (true)
    {
        int input = cv::waitKey(10);
        char zhongduan;
        if (if_zhongduan != 0)
        {
            int re = read(STDIN_FILENO, &zhongduan, 1);
            if (re == 1)
                input = zhongduan;
        }
        if (input == 27)
            break;

        handleExposureKey(cam, input);

        if (input == 'a')
        {
            if (!cam.start())      // 开始抓帧
            {
                std::cout << "抓帧失败";
                break;
            }
            std::cout << "抓帧中：按 e 查询曝光；按 = 增加、- 减少曝光（每次 500 微秒）；按 s 停止抓帧；如需退出，请先按 s，再按 Esc。" << std::endl;
            while (true)
            {
                cv::Mat img;       // 装一帧图像
                if (cam.getimg(img))
                {
                    cv::imshow("Camera", img);
                }
                int input = cv::waitKey(10);
                if (if_zhongduan != 0)
                {
                    int re = read(STDIN_FILENO, &zhongduan, 1);
                    if (re == 1)
                        input = zhongduan;
                }
                handleExposureKey(cam, input);
                if (input == 's')
                {
                    break;
                }
                if (input == 27)
                {
                    std::cout << "请先停止";
                }
            }
            cam.stop();
            std::cout << "已停止抓帧。按 a 再次开始；按 e 查询曝光；按 = 增加、- 减少曝光；按 Esc 退出程序。" << std::endl;
        }
    }
    if (if_zhongduan != 0)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &yuan);
    }
    cv::destroyAllWindows();
    return 0;                      // Camera 析构自动清理
}
