cmake_minimum_required(VERSION 3.16)
project(camera LANGUAGES CXX)

# 为编辑器生成与实际构建一致的头文件搜索路径和编译参数。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc imgcodecs)
find_package(Threads REQUIRED)

find_path(MVS_INCLUDE_DIR MvCameraControl.h PATHS /opt/MVS/include REQUIRED NO_DEFAULT_PATH)
find_library(MVS_LIBRARY MvCameraControl PATHS /opt/MVS/lib/64 REQUIRED NO_DEFAULT_PATH)

add_executable(camera main.cpp camera.cpp SDKContext.cpp)
target_include_directories(camera PRIVATE "${MVS_INCLUDE_DIR}")
target_link_libraries(camera PRIVATE "${MVS_LIBRARY}" Threads::Threads ${OpenCV_LIBS})
set_target_properties(camera PROPERTIES BUILD_RPATH "/opt/MVS/lib/64")
