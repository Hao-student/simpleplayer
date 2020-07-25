# simpleplayer  
## 简介  
1.`leiaudio.cpp`和`leiplayer.cpp`  
雷神的ffmpeg+SDL2音/视频播放器代码，使用SDL事件，窗口可关闭，作参考学习  
2.`mp4player.cpp`和`mp4player.h`  
自己尝试的ffmpeg+SDL2画面播放器（无声音），窗口不可关闭  
3.`zhplayer.cpp`、`zhplayer.h`和`Queue.h`  
参考雷神程序和部分CSDN博客，实现音视频同步播放，窗口不可关闭  
## 使用说明  
1.上传的`CMakeLists.txt`中使用的是`zhplayer.cpp`为主程序  
2.编译后生成`mp4player.exe`执行文件，需要手动输入播放文件路径，也可直接在cpp中改为本地连接  
3.如果想要使用简介中1或者2代码，需要更改`CMakeLists.txt`
4.如需更改成1中音频播放器，将`CMakeLists.txt`中
`add_executable (mp4player zhplayer.cpp)`改为`add_executable (mp4player leiaudio.cpp)`即可  
5.如需改为2，将`CMakeLists.txt`中`add_executable (mp4player zhplayer.cpp)`进行注释  
`#add_executable (playertest mp4player.cpp mp4player.h)`取消注释，并将下方解决方案名称进行修改即可  
