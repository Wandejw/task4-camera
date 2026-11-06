# Task4 · 工业相机采集

RobotPilots 算法组训练项目，使用 C++、海康 MVS SDK 和 OpenCV 完成工业相机采集与显示。

## 功能与训练内容

- 枚举相机并按编号选择，打开、开始采集、停止采集和释放资源。
- 将采集图像转换为 OpenCV 图像并实时显示。
- 查询和调整曝光时间。
- 练习相机 SDK 封装、资源生命周期管理，以及采集与显示之间的数据共享。

## 运行

依赖 Linux、CMake、OpenCV 和海康 MVS SDK；当前构建配置使用 `/opt/MVS`。

```bash
cmake -S . -B build
cmake --build build
./build/camera
```

连接相机后选择编号：`a` 开始采集，`s` 停止，`e` 查询曝光，`=`／`-` 调整曝光；停止采集后按 `Esc` 退出。
