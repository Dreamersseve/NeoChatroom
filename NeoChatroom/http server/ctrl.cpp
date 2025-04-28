#include "../include/Server.h"
#include "../include/log.h"
#include "../include/datamanage.h"
#include "server/chatroom.h" 
#include "server/roommanager.h"
#include "../json/json.h"

#include <string>
#include <thread>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>
#include <stdexcept>

using namespace std;

// Ĭ�Ϸ���������
static std::string CURRENT_HOST = "0.0.0.0";
static int CURRENT_PORT = 80;

// Configuration file path
const string CONFIG_FILE = "./config.json";

// Forward declaration for login system starter
void start_loginSystem();

// Dummy implementation for UTF8 conversion (modify if needed)
std::string convertToUTF8(const std::string& input) {
    return input;
}

// -------------------------------------------------------------------
// Configuration file related functions using jsoncpp
// -------------------------------------------------------------------
void saveConfig() {
    Json::Value root;
    Json::Value rooms(Json::arrayValue);
    // ����������ʹ�õ������ң�����Ϣд�������ļ�
    for (int i = 0; i < MAXROOM; i++) {
        if (used[i]) {
            Json::Value roomObj;
            roomObj["id"] = i;
            // �ٶ��������� gettittle() ������ȡ��ǰ����
            roomObj["name"] = room[i].gettittle();
            // �������������루���� GetPassword ��ȡ��
            roomObj["password"] = room[i].GetPassword();
            rooms.append(roomObj);
        }
    }
    root["rooms"] = rooms;

    // ������������ã�HOST �� PORT
    root["server"]["host"] = CURRENT_HOST;
    root["server"]["port"] = CURRENT_PORT;

    ofstream ofs(CONFIG_FILE);
    if (ofs.is_open()) {
        ofs << root;
        ofs.close();
    }
}

void loadConfig() {
    ifstream ifs(CONFIG_FILE);
    if (!ifs.is_open()) {
        // �����ļ������ڣ��������
        return;
    }
    Json::Value root;
    ifs >> root;
    ifs.close();

    // �ȼ��ط��������ã�������������ȫ�ֱ��������õ�������ʵ���ϣ�
    if (!root["server"].isNull()) {
        if (root["server"].isMember("host"))
            CURRENT_HOST = root["server"]["host"].asString();
        if (root["server"].isMember("port"))
            CURRENT_PORT = root["server"]["port"].asInt();

        // ��ȡ������ʵ��������HOST��PORT
        Server& server = Server::getInstance();
        server.setHOST(CURRENT_HOST);
        server.setPORT(CURRENT_PORT);
    }

    // ��������������
    const Json::Value rooms = root["rooms"];
    for (const auto& roomObj : rooms) {
        int id = roomObj["id"].asInt();
        string name = roomObj["name"].asString();
        if (id >= 0 && id < MAXROOM) {
            // �����������δ���������Դ���
            if (!used[id]) {
                int newId = addroom();
                while (newId < id) {
                    newId = addroom();
                }
            }
            // ����������������������
            editroom(id, name);
            // �������а��������ֶΣ�����������������
            if (roomObj.isMember("password")) {
                string pwd = roomObj["password"].asString();
                room[id].setPassword(pwd);
            }

            // �Զ�����������
            if (!room[id].start()) {
                Logger::getInstance().logError("Control", "�޷����������ң�ID: " + to_string(id));
            }
        }
    }
}

// -------------------------------------------------------------------
// Command runner functions: loop and execute commands.
// ���ñ��潫�ڴ�����ɾ����������������ʱ������
// -------------------------------------------------------------------
void command_runner(string command, int roomid) {
    // ȥ������ĩβ����ո�
    while (!command.empty() && command.back() == ' ') {
        command.pop_back();
    }

    Logger& logger = Logger::getInstance();
    // ע������Ϊ�˱���ԭʼ�߼�����Ȼʹ��Ĭ��HOST������÷�����ʵ��
    Server& server = Server::getInstance(HOST);
    try {
        // �������Ϊ������Ͳ���
        string cmd = command;
        string args = "";
        size_t spacePos = command.find(' ');
        if (spacePos != string::npos) {
            cmd = command.substr(0, spacePos);
            args = command.substr(spacePos + 1);
            // ȥ��������β�Ŀո�
            while (!args.empty() && args.front() == ' ') args.erase(0, 1);
            while (!args.empty() && args.back() == ' ') args.pop_back();
        }

        if (command == "userdata save") {
            manager::WriteUserData("./", manager::datafile);
            logger.logInfo("Control", "�����ѱ���");
        }
        else if (command == "userdata load") {
            manager::ReadUserData("./", manager::datafile);
            logger.logInfo("Control", "�����Ѽ���");
        }
        else if (command == "start") {  // ����ƥ�� start ����
            logger.logInfo("Control", "�������ѿ���");
            start_manager();
            start_loginSystem();
            manager::ReadUserData("./", manager::datafile);
            // �������ã���������������ͷ���������
            loadConfig();
            server.start();
        }
        else if (cmd == "create") {
            int newRoomId = addroom();
            if (newRoomId != -1) {
                // �Զ������´�����������
                if (room[newRoomId].start()) {
                    logger.logInfo("Control", "�������Ѵ�����������ID: " + to_string(newRoomId));
                    saveConfig();  // �����󱣴�����
                }
                else {
                    logger.logError("Control", "�������Ѵ������޷�������ID: " + to_string(newRoomId));
                }
            }
            else {
                logger.logError("Control", "�޷����������ң��Ѵﵽ�������");
            }
        }
        else if (cmd == "delete" && !args.empty()) {
            int roomId = stoi(args);
            if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                delroom(roomId);
                logger.logInfo("Control", "��������ɾ����ID: " + to_string(roomId));
                saveConfig();  // ɾ���󱣴�����
            }
            else {
                logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
            }
        }
        else if (cmd == "settype" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                int neetype = atoi(args.substr(spacePos + 1).c_str());
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    setroomtype(roomId, neetype);
                    logger.logInfo("Control", "�����������Ѹ��� " + to_string(roomId) + "��������: " + to_string(neetype));
                    // ����ѡ�����������Ҫ���棬����� saveConfig() 
                }
                else {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "settype �����ʽ������Ҫ�ṩ ID ��������");
            }
        }
        else if (cmd == "rename" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                string newName = args.substr(spacePos + 1);
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    editroom(roomId, newName);
                    logger.logInfo("Control", "����������������ID: " + to_string(roomId) + "��������: " + newName);
                    saveConfig();  // �������󱣴�����
                }
                else {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "rename �����ʽ������Ҫ�ṩ ID ��������");
            }
        }
        else if (cmd == "stop") {
            manager::WriteUserData("./", manager::datafile);
            exit(0);
        }
        else if (cmd == "say" && !args.empty()) {
            size_t msgPos = args.find(' ');
            if (msgPos != string::npos) {
                int roomId = stoi(args.substr(0, msgPos));
                string message = args.substr(msgPos + 1);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    string UTF8msg = message;
                    string GBKmsg = WordCode::Utf8ToGbk(UTF8msg.c_str());
                    room[roomId].systemMessage(convertToUTF8(GBKmsg));
                    logger.logInfo("Control", "��Ϣ�ѷ��͵������� " + to_string(roomId));
                }
                else {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "�����ʽ����: say <roomId> <message>");
            }
        }
        else if (cmd == "clear" && !args.empty()) {
            int roomId = stoi(args);
            if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                room[roomId].clearMessage();
                logger.logInfo("Control", "������ " + to_string(roomId) + " ����Ϣ�б������");
            }
            else {
                logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
            }
        }
        else if (cmd == "ban" && !args.empty()) {
            string ip = args;
            server.banIP(args);
        }
        else if (cmd == "deban" && !args.empty()) {
            string ip = args;
            server.debanIP(args);
        }
        else if (cmd == "setpassword" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                string password = args.substr(spacePos + 1);
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    room[roomId].setPassword(password);
                    logger.logInfo("Control", "���������������ã�ID: " + to_string(roomId));
                    saveConfig();
                }
                else {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "setpassword �����ʽ������Ҫ�ṩ ID ������");
            }
        }
        else if (cmd == "listuser") {
            auto userDetails = manager::GetUserDetails();
            if (userDetails.empty()) {
                logger.logInfo("Control", "��ǰû���û�");
            } else {
                logger.logInfo("Control", "�û��б�:");
                for (const auto& [name, password, uid] : userDetails) {
                    logger.logInfo("Control", "�û���: " + name + ", ����: " + password + ", UID: " + to_string(uid));
                }
            }
        }
        else if (cmd == "listroom") {
            auto roomDetails = GetRoomListDetails(); 
            if (roomDetails.empty()) {
                logger.logInfo("Control", "��ǰû��������");
            } else {
                logger.logInfo("Control", "�������б�:");
                for (const auto& [id, name, password] : roomDetails) {
                    logger.logInfo("Control", "ID: " + to_string(id) + ", ����: " + name + ", ����: " + password);
                }
            }
        }
        else if (cmd == "help") {
            logger.logInfo("Control", "����ָ��:\n"
                "  userdata save - �����û�����\n"
                "  userdata load - �����û�����\n"
                "  start - ����������\n"
                "  stop - ֹͣ������\n"
                "  create - ����������һ���µ�������\n"
                "  delete <roomId> - ɾ��ָ��������\n"
                "  rename <roomId> <name> - Ϊָ��������������\n"
                "  settype <roomId> <type> - Ϊָ�������Ҹ������� 2-��ֹ���� 3-����\n"
                "  say <roomId> <message> - ��ָ�������ҷ�����Ϣ\n"
                "  clear <roomId> - ���ָ�������ҵ���Ϣ\n"
                "  ban <ip> - ���ָ��IP\n"
                "  deban <ip> - ���ָ��IP\n"
                "  setpassword <roomId> <password> - Ϊָ����������������\n"
                "  listuser - �г������û�\n"
                "  listroom - �г�����������\n"
                "  help - ��ʾ�˰�����Ϣ");
                }
        else {
            logger.logError("Control", "���Ϸ���ָ��: " + command);
        }
    }
    catch (const exception& e) {
        logger.logFatal("Control", "����ִ��ʧ��: " + string(e.what()));
    }
}

void command() {
    while (true) {
        string cmd;
        getline(cin, cmd);
        // ÿ��������ڷ����߳��д���
        thread cmd_thread(command_runner, cmd, 0);
        cmd_thread.detach();
    }
}

void run() {
    // ��ʼ������������Ĭ��������
    //addroom(); // ������һ��������
    //addroom(); // �����ڶ���������
    // ����ʱ�������ã��������������ü����������ã�
    loadConfig();
    thread maint(command);
    maint.join();
    Logger& logger = Logger::getInstance();
    logger.logInfo("Control", "�����߳��ѽ���");
    return;
}
