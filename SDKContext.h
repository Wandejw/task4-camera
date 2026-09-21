#ifndef SDK_CONTEXT_H
#define SDK_CONTEXT_H

// 管理整个海康 SDK 的生命周期，不管理单个相机。
class SDKContext
{
public:
    SDKContext();                 // 初始化 SDK
    ~SDKContext();                // 反初始化 SDK

    SDKContext(const SDKContext&) = delete;
    SDKContext& operator=(const SDKContext&) = delete;
};

#endif // SDK_CONTEXT_H
