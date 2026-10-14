# MP4 转 MP3

一个用 C++ 编写的 Windows 原生窗口小工具，通过独立 FFmpeg 进程提取视频音轨。

支持把 MP4 拖进窗口、手动输入或浏览文件；可指定 MP3 文件名或输出目录。输出留空时保存在原视频目录，已有同名文件时提示更换名称，转换进程使用 `-n` 避免覆盖。

## 构建

需要 Windows、C++17 编译器和 CMake：

```sh
cmake -S . -B build
cmake --build build --config Release
```

或使用 MinGW-w64：

```sh
g++ -std=c++17 -municode -mwindows -DUNICODE -D_UNICODE src/mp4_to_mp3.cpp -o mp4_to_mp3.exe -lcomdlg32 -lshell32 -lgdi32
```

自行安装带 `libmp3lame` 编码器的 FFmpeg，将 `ffmpeg.exe` 放在程序旁边或加入 PATH。

## 公开范围与限制

此仓库保留原工具源码，新增构建说明；原自定义图标的分发许可未确认，因此图标、图标制作脚本和旧二进制均不包含。源码仍尝试加载资源编号 1；没有该资源时使用系统默认窗口表现，不影响转换入口。

该工具只提取音频，没有语音识别、字幕翻译或文档整理功能。仅处理单个文件，当前界面不提供任务取消和详细 FFmpeg 错误日志。依赖项动态库请按所用编译器的分发要求准备。

本次公开整理验证了源码可编译；原开发记录曾验证测试 MP4 到 MP3 转换。本次未重复进行图形界面运行验收。

## 许可证

自写源码采用 MIT。FFmpeg 不随此仓库分发，也不适用本仓库的 MIT 许可；具体构建可能采用 LGPL 或 GPL，应查看所安装版本的许可。见 [FFmpeg 官方法律说明](https://ffmpeg.org/legal.html)。
