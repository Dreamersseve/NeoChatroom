#include "../../include/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "../../json/json.h"
#include "../../include/datamanage.h"
#include "../../include/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"

// Ensure room initialization logic is robust
vector<chatroom> room(MAXROOM);
bool used[MAXROOM] = { false };

int addroom() {
    for (int i = 1; i < MAXROOM; i++) {
        if (!used[i]) {
            used[i] = true;
            room[i].init();
            room[i].setRoomID(i);
            return i;
        }
    }
    return -1;
}

void delroom(int x) {
    if (x >= 1 && x < MAXROOM && used[x]) {
        used[x] = false;
        room[x].init();
    }
}

int editroom(int x, string Roomtittle) {
    if (x >= room.size()) return 1;
    room[x].settittle(Roomtittle);
    return 0;
}

int setroomtype(int x, int type) {
    if (x >= room.size()) return 1;
    room[x].settype(type);
    return 0;
}

/*
 * Modified transCookie to handle cookie strings that may simply be the UID
 * instead of the expected "uid=..." key=value format.
 */
void transCookie(std::string& cid, std::string& uid, const std::string& cookie) {
    // First, attempt to extract clientid if present.
    std::string::size_type pos1 = cookie.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookie.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookie.length();
        cid = cookie.substr(pos1, pos2 - pos1);
    }

    // Attempt to extract uid using "uid=" pattern.
    std::string::size_type pos3 = cookie.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookie.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookie.length();
        uid = cookie.substr(pos3, pos4 - pos3);
    }
    else {
        // If no "uid=" pattern found and cookie does not contain "=",
        // assume the entire cookie string is the uid.
        if (cookie.find('=') == std::string::npos && !cookie.empty()) {
            uid = cookie;
        }
    }
}

string getRoomName(int roomid) {
    return room[roomid].gettittle();
}

void getRoomList(const httplib::Request& req, httplib::Response& res) {
    // ��ȡ Cookie ������
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);

    // If uid is still empty and cookies is non-empty, try to use cookies as uid.
    if (uid.empty() && !cookies.empty() && cookies.find('=') == std::string::npos) {
        uid = cookies;
    }

    // ת�� uid Ϊ����
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        res.status = 400; // Bad Request
        res.set_content("Invalid UID format", "text/plain");
        return;
    }

    // �����û�
    manager::user* user = manager::FindUser(uid_);
    if (user == nullptr) {
        res.status = 404; // Not Found
        res.set_content("User not found", "text/plain");
        return;
    }

    // ��ȡ�û��������� ID �б�
    std::string List = user->getcookie();
    if (List.empty()) {
        res.status = 200; // OK
        res.set_content("[]", "application/json"); // ���ؿյ� JSON ����
        return;
    }

    // �ָ� List �ַ�������ȡÿ�������� ID
    std::vector<std::string> roomIds;
    std::stringstream ss(List);
    std::string roomId;
    while (std::getline(ss, roomId, '&')) {
        roomIds.push_back(roomId);
    }

    // ����һ�� JSON ��Ӧ����
    Json::Value response(Json::arrayValue);

    // �������������� ID����ȡ��Ӧ������������
    for (const std::string& roomId : roomIds) {
        int room_id;
        if (!str::safeatoi(roomId, room_id)) {
            continue; // ������Ч�������� ID
        }

        std::string roomName = getRoomName(room_id);

        // ����ÿ�������ҵ� JSON ����
        Json::Value room;
        room["id"] = roomId;  // ��������ҵ� ID
        room["name"] = roomName;  // ��������ҵ�����

        response.append(room);  // �������Ҷ�����ӵ� JSON ������
    }

    // ������Ӧͷ��֧�ֿ���
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");

    // ������Ӧ����Ϊ JSON ��ʽ���ַ���
    res.set_content(response.toStyledString(), "application/json");
}

void getAllList(const httplib::Request& req, httplib::Response& res) {
    // ���� JSON ����������ڴ洢����Ҫ���ص�������
    Json::Value response(Json::arrayValue);

    // �������������ң����������ұ�Ŵ� 1 ��ʼ��
    for (int i = 1; i < MAXROOM; i++) {
        // �����������δ��ʹ�ã�������
        if (!used[i]) continue;
        // ��������ҵ� getype ���� 3�����������ң�������
        if (room[i].gettype() == 3) continue;

        // ���������� JSON ��������ֻ���� id �� tittle��
        Json::Value roomObj;
        roomObj["id"] = std::to_string(i);
        roomObj["name"] = room[i].gettittle();
        response.append(roomObj);
    }

    // ������Ӧͷ��֧�ֿ���
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}

void AddRoomToUser(int uid, int roomId) {
    // �����û�
    manager::user* user = manager::FindUser(uid);
    if (user == nullptr) {
        // �û������ڣ�ֱ�ӷ���
        return;
    }

    // ��ȡ��ǰ�� cookie �ַ���
    std::string cookie = user->getcookie();

    // ��roomIdת���ַ���
    std::string roomIdStr = std::to_string(roomId);

    // ����Ƿ��Ѿ����ڣ������ظ����
    std::stringstream ss(cookie);
    std::string token;
    while (std::getline(ss, token, '&')) {
        if (token == roomIdStr) {
            // �Ѿ����ڣ�����Ҫ�����
            return;
        }
    }

    // ���ԭ�������ݣ���ӷָ���
    if (!cookie.empty()) {
        cookie += "&";
    }
    cookie += roomIdStr;

    // �����û�cookie
    user->setcookie(cookie);
}

// ����·�� /addroom�����ڼ���������
void addRoomToUserRoute(const httplib::Request& req, httplib::Response& res) {
    // ��ȡ Cookie �������û� uid
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        res.status = 400;
        res.set_content("Invalid UID format", "text/plain");
        return;
    }
    manager::user* user = manager::FindUser(uid_);
    if (user == nullptr) {
        res.status = 404;
        res.set_content("User not found", "text/plain");
        return;
    }
    // ��ȡ������� roomId������ /addroom?roomId=5��
    if (!req.has_param("roomId")) {
        res.status = 400;
        res.set_content("Missing roomId parameter", "text/plain");
        return;
    }
    std::string roomIdStr = req.get_param_value("roomId");
    int roomId;
    if (!str::safeatoi(roomIdStr, roomId)) {
        res.status = 400;
        res.set_content("Invalid roomId format", "text/plain");
        return;
    }
    // �����������Ƿ��������ʹ��
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        res.status = 404;
        res.set_content("Room not found", "text/plain");
        return;
    }
    // ��������ҵ� gettype ���� 2���򲻿ɼ���
    if (room[roomId].gettype() == 2) {
        res.status = 403;
        res.set_content("Cannot join this room", "text/plain");
        return;
    }
    // ���� AddRoomToUser �������û��� cookie�������ظ��������ڲ����жϣ�
    AddRoomToUser(uid_, roomId);
    res.status = 200;
    res.set_content("Joined successfully", "text/plain");
}

void start_manager() {
    Server& server = Server::getInstance(HOST);
    server.handleRequest("/list", getRoomList);
    server.handleRequest("/allchatlist", getAllList);
    server.handleRequest("/addroom", addRoomToUserRoute);
}