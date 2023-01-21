### 功能

-   为每个用户在每天的00:05~00:10随机选择一个时刻打卡，填报内容与上一次填报一致
-   任何用户可查看自己的日志
    -   Skip：将下次打卡推迟一天
    -   Quit：退出自动打卡服务
    -   Source：本项目开源地址
-   `ADMINISTRATOR`可查看所有用户当天打卡情况

### 部署

0.   确保系统时间是准确的，确保g++版本在4.8以上

1.  安装OpenSSL 1.1.1 or 3.0.x
2.  配置pch.h
    -   `CA_CERT_DIR_PATH`：对于Linux系统，这一项是CA证书的存放目录，视具体系统而定；对于Windows系统，请无视这一项
    -   `SVR_CERT_PATH`：服务器证书（链）文件，须为PEM格式
    -   `SVR_KEY_PATH`：服务器私钥文件，须为PEM格式
    -   `ADMINISTRATOR`：服主学号（不必需）
3.  编译
    -   Linux: `g++ main.cpp usrdb.cpp schedule.cpp web.cpp -O2 -std=c++11 -lpthread -lssl -lcrypto -o ancov`
    -   Windows: `g++ main.cpp usrdb.cpp schedule.cpp web.cpp -O2 -std=c++11 -lpthread -lssl -lcrypto -lws2_32 -lcrypt32 -o ancov -I "xxxxx\OpenSSL-Win64\include" -L "xxxxx\OpenSSL-Win64\lib"`
4.  运行

### 使用

浏览器访问`https://xxxxx/clockin`即可。初次使用会跳转到登录页面。

（仅限`ADMINISTRATOR`）查看所有用户当天打卡情况：浏览器访问`https://xxxxx/clockin/overview`。

### 命令行

`list failed`：打印所有打卡失败的用户

`clockin admin`：立刻为`ADMINISTRATOR`打卡

`clockin failed`：立刻为所有打卡失败的用户打卡

`exit`：退出程序
