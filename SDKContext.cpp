#include "SDKContext.h"
#include "MvCameraControl.h"
#include <iostream>

// 初始化海康 MVS SDK。
SDKContext::SDKContext()
{
    int nRet = MV_CC_Initialize();
    if (MV_OK != nRet)
    {
        std::cout << "SDK 初始化失败";
    }
}

// 反初始化海康 MVS SDK。
SDKContext::~SDKContext()
{
    MV_CC_Finalize();
}
