# Qt UVC Camera

- Qt 中实现摄像头的预览/拍图/录像等操作，因为 QCamera 的接口不完善且 BUG 多，故自己根据平台或第三方库进行实现。

# 开发环境

- Win10/Win11 + VS2019 + Qt5.15.2 32/64bit

# 项目结构

- DSCamera：Windows DirectShow 实现的 Camera

# 注意事项

- 目前只测试了 YUY2 和 MJPG 格式，其他格式没有设备来测试
