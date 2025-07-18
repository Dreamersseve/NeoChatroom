#ifndef CHATROOM_H
#define CHATROOM_H

#include "../server/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "json/json.h"
#include "../data/datamanage.h"
#include "../tool/tool.h"
#include <mutex>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "../../../../../lib/httplib.h"
#include <atomic>

// 前向声明
class PrivateChat;

using namespace std;
const int MAXROOM = 1000;
class chatroom {
public:
    vector<int> allowID;
    // 声明友元类，使PrivateChat可以访问私有成员
    friend class PrivateChat;
private:
    // Maximum number of messages to store in the chat history
    const size_t MAXSIZE = 100;  // Updated comment to clarify usage
    deque<Json::Value> chatMessages;
    string chatTitle;
    int roomid;

    string passwordHash;

    unsigned int flags = 0;
    int type;// 3为隐藏 2为禁止加入 已弃用
    // Removed unused type variable

    // 初始化聊天室
    void initializeChatRoom();

    // 将 Json 消消息转为字符串
    string transJsonMessage(Json::Value m);

    //检查请求是否在许可名单内
    bool checkAllowId(const int ID);
    bool checkAllowId(const string name);

    // 解析 Cookie
    void transCookie(std::string& cid, std::string& uid, std::string cookie);

    // 检查Cooie
    bool checkCookies(const httplib::Request& req);

    // 设置静态文件路由
    void setupStaticRoutes();

    // 判断是否是有效图片类型
    bool isValidImage(const std::string& filename);

    // 设置聊天相关路由
    void setupChatRoutes();

    // 用于同步的锁
    std::mutex mtx;

    const std::vector<std::string> allowedImageTypes = { ".jpg", ".jpeg", ".png", ".gif", ".bmp" ,"webp" };

    std::string password; // Store the password for the chatroom.
    
    // 懒加载相关成员变量
    std::atomic<time_t> lastAccessTime;  // 最后访问时间
    std::atomic<bool> isActive;          // 聊天室是否已激活

public:
    // 获取聊天消息
    void getChatMessages(const httplib::Request& req, httplib::Response& res);

    // 获取全部聊天消息
    void getAllChatMessages(const httplib::Request& req, httplib::Response& res);

    // 处理 POST 请求发送消息
    void postChatMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root);

    // 上传图片
    void uploadImage(const httplib::Request& req, httplib::Response& res);

    // 构造函数和析构函数
    //chatroom(int id) : roomid(id) {}
    //~chatroom() {}

    // 系统消息
    void systemMessage(string message);

    //清空消息内容
    void clearMessage();

    // 启动聊天室
    bool start();

    void setflag(int x);

    string gettittle();

    void settittle(string Tittle);

    void setPasswordHash(const std::string& hash);
    std::string getPasswordHash() const;

    void setRoomID(int id);

    void init();

    // Method to set the password for the chatroom.
    void setPassword(const std::string& Newpassword);

    string GetPassword();

    enum RoomFlags { //标志
        ROOM_HIDDEN = 1 << 0,       // 0001
        ROOM_NO_JOIN = 1 << 1       // 0010

    };
    // 设置标志
    void setFlag(RoomFlags flag);

    // 清除标���
    void clearFlag(RoomFlags flag);

    // 检查标志
    bool hasFlag(RoomFlags flag) const;
    
    // 懒加载相关方法
    // 更新最后访问时间
    void updateAccessTime();
    
    // 获取最后访问时间
    time_t getLastAccessTime() const;
    
    // 检查聊天室是否已激活
    bool isActivated() const;
    
    // 停用聊天室（卸载）
    bool deactivate();
};

void delroom(int x);

#endif // CHATROOM_H