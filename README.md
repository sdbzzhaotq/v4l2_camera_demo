# v4l2_camera_demo
linux V4L2调用UVC摄像头QT示例,可预览、切换分辨率、获取和修改摄像头参数
# 环境
ubuntu18.04 + QT5.9.9
# 安装依赖库
sudo apt-get install libjpeg-dev libv4l-dev
# 使用
camera_demo.pro中添加libjpeg.so的路径： 将unix:LIBS += **/usr/lib/x86_64-linux-gnu/libjpeg.so**替换为自己的路径
# API
|                             接口                             |         功能         |
| :----------------------------------------------------------: | :------------------: |
|                     StartVideoPrePare()                      |       准备工作       |
|                      StartVideoStream()                      |      打开视频流      |
|                       EndVideoStream()                       |      关闭视频流      |
|                          GetFrame()                          |     获取一帧图片     |
| MJPEG2RGB(unsigned char* data_frame, unsigned char *rgb, int bytesused) |      MJPEG转RGB      |


**Note:**
-  GetFrame()是阻塞读取视频帧的，需在线程中调用
-  GetFrame()获取到视频帧后就可以实现更多的功能,如拍照、录像等

# 参考项目
- https://github.com/jacksonliam/mjpg-streamer
- https://github.com/ZXX521/V4L2-Qt5.6.0-Ubuntu16.04
