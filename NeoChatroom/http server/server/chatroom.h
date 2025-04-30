#ifndef CHATROOM_H
#define CHATROOM_H

#include "../../include/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "../../json/json.h"
#include "../../include/datamanage.h"
#include "../../include/tool.h"
#include <mutex>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "../../include/httplib.h"  // ������ʹ�õ��������

using namespace std;
const int MAXROOM = 1000;
class chatroom {
public:
    vector<int> allowID;
private:
    // Maximum number of messages to store in the chat history
    const size_t MAXSIZE = 100;  // Updated comment to clarify usage
    deque<Json::Value> chatMessages;
    string chatTitle;
    int roomid;

    string passwordHash;

    unsigned int flags = 0;
    int type;// 3Ϊ���� 2Ϊ��ֹ���� ������
    // Removed unused type variable

    // ��ʼ��������
    void initializeChatRoom();

    // �� Json ��ϢתΪ�ַ���
    string transJsonMessage(Json::Value m);

    //��������Ƿ������������
    bool checkAllowId(const int ID);
    bool checkAllowId(const string name);

    // ���� Cookie
    void transCookie(std::string& cid, std::string& uid, std::string cookie);

    // ���Cooie
    bool checkCookies(const httplib::Request& req);

    // ��ȡ������Ϣ
    void getChatMessages(const httplib::Request& req, httplib::Response& res);

    // ��ȡȫ��������Ϣ
    void getAllChatMessages(const httplib::Request& req, httplib::Response& res);

    // ���� POST ��������Ϣ
    void postChatMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root);

    // ��ȡ�û���
    void getUsername(const httplib::Request& req, httplib::Response& res);

    // ���þ�̬�ļ�·��
    void setupStaticRoutes();

    // �ж��Ƿ�����ЧͼƬ����
    bool isValidImage(const std::string& filename);

    // �ϴ�ͼƬ
    void uploadImage(const httplib::Request& req, httplib::Response& res);

    // �����������·��
    void setupChatRoutes();

    // ����ͬ������
    std::mutex mtx;

    const std::vector<std::string> allowedImageTypes = { ".jpg", ".jpeg", ".png", ".gif", ".bmp" ,"webp" };

    std::string password; // Store the password for the chatroom.

public:
    

    // ���캯������������
    //chatroom(int id) : roomid(id) {}
    //~chatroom() {}

    // ϵͳ��Ϣ
    void systemMessage(string message);

    //�����Ϣ����
    void clearMessage();

    // ����������
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

    enum RoomFlags { //��־
        ROOM_HIDDEN = 1 << 0,       // 0001
        ROOM_NO_JOIN = 1 << 1       // 0010

    };
    // ���ñ�־
    void setFlag(RoomFlags flag);

    // �����־
    void clearFlag(RoomFlags flag);

    // ����־
    bool hasFlag(RoomFlags flag) const;
};

void delroom(int x);

#endif // CHATROOM_H