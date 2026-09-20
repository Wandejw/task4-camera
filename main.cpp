// 上层交互：选择相机、接收按键、显示画面
#include "Camera.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <termios.h>
#include <unistd.h>

int main()
{
    Camera cam;                    // 创建（初始化 SDK）

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

    std::cout << "按 a 开始抓帧；按 Esc 退出程序。" << std::endl;
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

        if (input == 'a')
        {
            if (!cam.start())      // 开始抓帧
            {
                std::cout << "抓帧失败";
                break;
            }
            std::cout << "抓帧中：按 s 停止抓帧；如需退出，请先按 s，再按 Esc。" << std::endl;
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
            std::cout << "已停止抓帧。按 a 再次开始；按 Esc 退出程序。" << std::endl;
        }
    }
    if (if_zhongduan != 0)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &yuan);
    }
    cv::destroyAllWindows();
    return 0;                      // Camera 析构自动清理
}
