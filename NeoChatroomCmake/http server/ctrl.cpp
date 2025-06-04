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
#include "RedirectServer.h"
using namespace std;

// Ĭ�Ϸ���������
static std::string CURRENT_HOST = "0.0.0.0";
static int CURRENT_PORT = 443;

// Configuration file path
const string CONFIG_FILE = "./config.json";

// Forward declaration for login system starter
void start_loginSystem();


// -------------------------------------------------------------------
// Configuration file related functions using jsoncpp
// -------------------------------------------------------------------
void saveConfig() {
    Logger& logger = Logger::getInstance();
    try {
        Json::Value root;
        Json::Value rooms(Json::arrayValue);
        // ����������ʹ�õ������ң�����Ϣд�������ļ�
        for (int i = 0; i < MAXROOM; i++) {
            if (used[i]) {
                try {
                    Json::Value roomObj;
                    roomObj["id"] = i;
                    // �ٶ��������� gettittle() ������ȡ��ǰ����
                    roomObj["name"] = room[i].gettittle();
                    // �������������루���� GetPassword ��ȡ��
                    roomObj["password"] = room[i].GetPassword();
                    rooms.append(roomObj);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "������������Ϣʧ�ܣ�ID: " + to_string(i) + "������: " + e.what());
                    // ��������������������Ϣ
                }
                catch (...) {
                    logger.logError("Config", "������������Ϣʱ����δ֪����ID: " + to_string(i));
                }
            }
        }
        root["rooms"] = rooms;

        // ������������ã�HOST �� PORT
        root["server"]["host"] = CURRENT_HOST;
        root["server"]["port"] = CURRENT_PORT;

        ofstream ofs(CONFIG_FILE);
        if (!ofs.is_open()) {
            logger.logError("Config", "�޷��������ļ�����д��: " + CONFIG_FILE);
            return;
        }
        try {
            ofs << root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "д�������ļ�ʧ��: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Config", "д�������ļ�ʱ����δ֪����");
        }
        ofs.close();
    }
    catch (const std::exception& e) {
        logger.logFatal("Config", "��������ʱ�����쳣: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Config", "��������ʱ����δ֪�쳣");
    }
}

void loadConfig() {
    Logger& logger = Logger::getInstance();
    try {
        ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            // �����ļ������ڣ��������
            logger.logInfo("Config", "�����ļ������ڣ���������: " + CONFIG_FILE);
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "���������ļ�ʧ��: " + string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logError("Config", "���������ļ�ʱ����δ֪����");
            ifs.close();
            return;
        }
        ifs.close();

        // �ȼ��ط��������ã�������������ȫ�ֱ��������õ�������ʵ���ϣ�
        if (!root["server"].isNull()) {
            try {
                if (root["server"].isMember("host"))
                    CURRENT_HOST = root["server"]["host"].asString();
                if (root["server"].isMember("port"))
                    CURRENT_PORT = root["server"]["port"].asInt();
            }
            catch (const std::exception& e) {
                logger.logError("Config", "��ȡ����������ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "��ȡ����������ʱ����δ֪����");
            }

            try {
                // ��ȡ������ʵ��������HOST��PORT
                Server& server = Server::getInstance();
                server.setHOST(CURRENT_HOST);
                server.setPORT(CURRENT_PORT);
            }
            catch (const std::exception& e) {
                logger.logError("Config", "���÷�����HOST/PORTʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "���÷�����HOST/PORTʱ����δ֪����");
            }
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "��������ʱ�����쳣: " + string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "��������ʱ����δ֪�쳣");
    }
}

void loadChatroomInConfig() {
    Logger& logger = Logger::getInstance();
    try {
        // ��������������
        ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            // �����ļ������ڣ��������
            logger.logInfo("Config", "�����ļ������ڣ����������Ҽ���: " + CONFIG_FILE);
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "��������������ʧ��: " + string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logError("Config", "��������������ʱ����δ֪����");
            ifs.close();
            return;
        }
        ifs.close();

        const Json::Value rooms = root["rooms"];
        for (const auto& roomObj : rooms) {
            try {
                int id = roomObj["id"].asInt();
                string name = roomObj["name"].asString();
                if (id < 0 || id >= MAXROOM) {
                    logger.logError("Config", "��Ч�������� ID (�����ļ�): " + to_string(id));
                    continue;
                }
                // �����������δ���������Դ���
                if (!used[id]) {
                    int newId = -1;
                    try {
                        newId = addroom();
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "����������ʧ�� (ID: " + to_string(id) + "): " + string(e.what()));
                        continue;
                    }
                    catch (...) {
                        logger.logError("Config", "����������ʱ����δ֪���� (ID: " + to_string(id) + ")");
                        continue;
                    }

                    while (newId < id && newId != -1) {
                        try {
                            newId = addroom();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Config", "��������������ʧ�� (���� ID: " + to_string(id) + "): " + string(e.what()));
                            break;
                        }
                        catch (...) {
                            logger.logError("Config", "��������������ʱ����δ֪���� (���� ID: " + to_string(id) + ")");
                            break;
                        }
                    }
                }

                // ����������������������
                try {
                    editroom(id, name);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "�༭����������ʧ�� (ID: " + to_string(id) + "): " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Config", "�༭����������ʱ����δ֪���� (ID: " + to_string(id) + ")");
                }

                // �������а��������ֶΣ�����������������
                if (roomObj.isMember("password")) {
                    try {
                        string pwd = roomObj["password"].asString();
                        room[id].setPassword(pwd);
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "��������������ʧ�� (ID: " + to_string(id) + "): " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Config", "��������������ʱ����δ֪���� (ID: " + to_string(id) + ")");
                    }
                }

                // �Զ�����������
                try {
                    if (!room[id].start()) {
                        logger.logError("Control", "�޷����������ң�ID: " + to_string(id));
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "����������ʱ�׳��쳣��ID: " + to_string(id) + "������: " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Control", "����������ʱ����δ֪����ID: " + to_string(id));
                }
            }
            catch (const std::exception& e) {
                logger.logError("Config", "��������������ʱ�����쳣: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "��������������ʱ����δ֪����");
            }
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "��������������ʱ�����쳣: " + string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "��������������ʱ����δ֪�쳣");
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

        if (command == "start") {  // ����ƥ�� start ����
            logger.logInfo("Control", "���Կ���������...");
            try {
                std::thread redirectThread([]() {
                    try {
                        Redirection::startRedirectServer();
                    }
                    catch (const std::exception& e) {
                        Logger::getInstance().logError("Redirect", "�ض������������ʧ��: " + string(e.what()));
                    }
                    catch (...) {
                        Logger::getInstance().logError("Redirect", "�ض������������ʱ����δ֪����");
                    }
                    });
                server.start();
                redirectThread.join();
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "����������ʱ�����쳣: " + string(e.what()));
                return;
            }
            catch (...) {
                logger.logFatal("Control", "����������ʱ����δ֪�쳣");
                return;
            }
        }
        else if (command == "load") {  // ����ƥ�� load ����
            logger.logInfo("Control", "���Լ�������...");
            try {
                // Initialize the database
                manager::InitDatabase("./database.db");
                logger.logInfo("Control", "���ݿ��ѳ�ʼ��");
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "���ݿ��ʼ��ʧ��: " + string(e.what()));
                return;
            }
            catch (...) {
                logger.logFatal("Control", "���ݿ��ʼ��ʱ����δ֪����");
                return;
            }

            try {
                start_loginSystem();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "������¼ϵͳʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "������¼ϵͳʱ����δ֪����");
            }

            try {
                start_manager();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "��������ϵͳʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "��������ϵͳʱ����δ֪����");
            }

            loadChatroomInConfig();
        }
        else if (cmd == "create") {
            try {
                int newRoomId = addroom();
                if (newRoomId != -1) {
                    // �Զ������´�����������
                    try {
                        if (room[newRoomId].start()) {
                            logger.logInfo("Control", "�������Ѵ�����������ID: " + to_string(newRoomId));
                            saveConfig();  // �����󱣴�����
                        }
                        else {
                            logger.logError("Control", "�������Ѵ������޷�������ID: " + to_string(newRoomId));
                        }
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "������������ʧ�ܣ�ID: " + to_string(newRoomId) + "������: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "������������ʱ����δ֪����ID: " + to_string(newRoomId));
                    }
                }
                else {
                    logger.logError("Control", "�޷����������ң��Ѵﵽ�������");
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "ִ�� create ����ʱ�����쳣: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "ִ�� create ����ʱ����δ֪����");
            }
        }
        else if (cmd == "delete" && !args.empty()) {
            try {
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
            catch (const std::invalid_argument&) {
                logger.logError("Control", "delete ���������ʽ����, ID ����Ϊ����");
            }
            catch (const std::exception& e) {
                logger.logError("Control", "ִ�� delete ����ʱ�����쳣: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "ִ�� delete ����ʱ����δ֪����");
            }
        }
        else if (cmd == "settype" && !args.empty()) {
            size_t spacePos2 = args.find(' ');
            if (spacePos2 != string::npos) {
                string idPart = args.substr(0, spacePos2);
                string typePart = args.substr(spacePos2 + 1);
                int roomId = -1, newType = -1;

                try {
                    roomId = stoi(idPart);
                    newType = stoi(typePart);
                }
                catch (const std::exception&) {
                    logger.logError("Control", "settype �����ʽ����ID �����ͱ���Ϊ����");
                    return;
                }

                if (roomId < 0 || roomId >= MAXROOM) {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
                else if (!used[roomId]) {
                    logger.logError("Control", "������δ������ID: " + to_string(roomId));
                }
                else {
                    try {
                        setroomtype(roomId, newType);
                        logger.logInfo("Control", "�����������Ѹ��£�ID: " + to_string(roomId) + "��������: " + to_string(newType));
                        // Optionally save configuration if type changes need persistence
                        saveConfig();
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "��������������ʧ�ܣ�ID: " + to_string(roomId) + "������: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "��������������ʱ����δ֪����ID: " + to_string(roomId));
                    }
                }
            }
            else {
                logger.logError("Control", "settype �����ʽ������Ҫ�ṩ ID ������");
            }
        }
        else if (cmd == "rename" && !args.empty()) {
            size_t spacePos3 = args.find(' ');
            if (spacePos3 != string::npos) {
                string idPart = args.substr(0, spacePos3);
                string newName = args.substr(spacePos3 + 1);
                try {
                    int roomId = stoi(idPart);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            editroom(roomId, newName);
                            logger.logInfo("Control", "����������������ID: " + to_string(roomId) + "��������: " + newName);
                            saveConfig();  // �������󱣴�����
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "������������ʧ�ܣ�ID: " + to_string(roomId) + "������: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "������������ʱ����δ֪����ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "rename ���������ʽ����ID ����Ϊ����");
                }
                catch (...) {
                    logger.logError("Control", "rename ����ʱ����δ֪����");
                }
            }
            else {
                logger.logError("Control", "rename �����ʽ������Ҫ�ṩ ID ��������");
            }
        }
        else if (cmd == "stop") {
            try {
                manager::CloseDatabase();
                logger.logInfo("Control", "���ݿ��ѹر�");
            }
            catch (const std::exception& e) {
                logger.logError("Control", "�ر����ݿ�ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "�ر����ݿ�ʱ����δ֪����");
            }
            exit(0);
        }
        else if (cmd == "say" && !args.empty()) {
            size_t msgPos = args.find(' ');
            if (msgPos != string::npos) {
                try {
                    int roomId = stoi(args.substr(0, msgPos));
                    string message = args.substr(msgPos + 1);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            room[roomId].systemMessage(message);
                            logger.logInfo("Control", "��Ϣ�ѷ��͵������� " + to_string(roomId));
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "����ϵͳ��Ϣʧ�ܣ������� ID: " + to_string(roomId) + "������: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "����ϵͳ��Ϣʱ����δ֪���������� ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "say ���������ʽ����ID ����Ϊ����");
                }
                catch (...) {
                    logger.logError("Control", "say ����ʱ����δ֪����");
                }
            }
            else {
                logger.logError("Control", "�����ʽ����: say <roomId> <message>");
            }
        }
        else if (cmd == "clear" && !args.empty()) {
            try {
                int roomId = stoi(args);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    try {
                        room[roomId].clearMessage();
                        logger.logInfo("Control", "������ " + to_string(roomId) + " ����Ϣ�б������");
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "�����Ϣʧ�ܣ������� ID: " + to_string(roomId) + "������: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "�����Ϣʱ����δ֪���������� ID: " + to_string(roomId));
                    }
                }
                else {
                    logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "clear ���������ʽ����ID ����Ϊ����");
            }
            catch (...) {
                logger.logError("Control", "clear ����ʱ����δ֪����");
            }
        }
        else if (cmd == "ban" && !args.empty()) {
            try {
                server.banIP(args);
                logger.logInfo("Control", "�ѷ�� IP: " + args);
            }
            catch (const std::exception& e) {
                logger.logError("Control", "��� IP ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "��� IP ʱ����δ֪����");
            }
        }
        else if (cmd == "deban" && !args.empty()) {
            try {
                server.debanIP(args);
                logger.logInfo("Control", "�ѽ�� IP: " + args);
            }
            catch (const std::exception& e) {
                logger.logError("Control", "��� IP ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "��� IP ʱ����δ֪����");
            }
        }
        else if (cmd == "setpassword" && !args.empty()) {
            size_t spacePos4 = args.find(' ');
            if (spacePos4 != string::npos) {
                string idPart = args.substr(0, spacePos4);
                string password = args.substr(spacePos4 + 1);
                try {
                    int roomId = stoi(idPart);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            if (password == "clear") {
                                room[roomId].setPassword(""); // �������
                                logger.logInfo("Control", "��������������գ�ID: " + to_string(roomId));
                            }
                            else {
                                room[roomId].setPassword(password); // ����������
                                logger.logInfo("Control", "���������������ã�ID: " + to_string(roomId));
                            }
                            saveConfig();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "��������������ʧ��, ID: " + to_string(roomId) + "������: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "��������������ʱ����δ֪����, ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "��Ч�������� ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "setpassword ���������ʽ����ID ����Ϊ����");
                }
                catch (...) {
                    logger.logError("Control", "setpassword ����ʱ����δ֪����");
                }
            }
            else {
                logger.logError("Control", "setpassword �����ʽ������Ҫ�ṩ ID ������");
            }
        }
        else if (cmd == "listuser") {
            try {
                auto userDetails = manager::GetUserDetails();
                if (userDetails.empty()) {
                    logger.logInfo("Control", "��ǰû���û�");
                }
                else {
                    logger.logInfo("Control", "�û��б�:");
                    for (const auto& [name, password, uid] : userDetails) {
                        logger.logInfo("Control", "�û���: " + name + ", ����: " + password + ", UID: " + to_string(uid));
                    }
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "��ȡ�û��б�ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "��ȡ�û��б�ʱ����δ֪����");
            }
        }
        else if (cmd == "listroom") {
            try {
                auto roomDetails = GetRoomListDetails();
                if (roomDetails.empty()) {
                    logger.logInfo("Control", "��ǰû��������");
                }
                else {
                    logger.logInfo("Control", "�������б�:");
                    for (const auto& [id, name, password] : roomDetails) {
                        logger.logInfo("Control", "ID: " + to_string(id) + ", ����: " + name + ", ����: " + password);
                    }
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "��ȡ�������б�ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "��ȡ�������б�ʱ����δ֪����");
            }
        }
        else if (cmd == "rmuser" && !args.empty()) {
            try {
                int userId = stoi(args);
                try {
                    if (manager::RemoveUser(userId)) {
                        logger.logInfo("Control", "�û��ѳɹ�ɾ����UID: " + to_string(userId));
                        //manager::WriteUserData("./", manager::datafile);
                    }
                    else {
                        logger.logError("Control", "ɾ���û�ʧ�ܣ������Ҳ���UID: " + to_string(userId));
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "ɾ���û�ʱ�����쳣��UID: " + to_string(userId) + "������: " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Control", "ɾ���û�ʱ����δ֪����UID: " + to_string(userId));
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "��Ч���û�ID��ʽ");
            }
            catch (...) {
                logger.logError("Control", "rmuser ����ʱ����δ֪����");
            }
        }
        else if (cmd == "help") {
            logger.logInfo("Control", "����ָ��:\n"
                "  start - ����������\n"
                "  stop - ֹͣ������\n"
                "  create - ����������һ���µ�������\n"
                "  delete <roomId> - ɾ��ָ��������\n"
                "  rename <roomId> <name> - Ϊָ��������������\n"
                "  settype <roomId> <type> - Ϊָ�������Ҹ������� (ROOM_HIDDEN = 1 << 0,ROOM_NO_JOIN = 1 << 1)\n"
                "  say <roomId> <message> - ��ָ�������ҷ�����Ϣ\n"
                "  clear <roomId> - ���ָ�������ҵ���Ϣ\n"
                "  ban <ip> - ���ָ��IP\n"
                "  deban <ip> - ���ָ��IP\n"
                "  setpassword <roomId> <password> - Ϊָ����������������\n"
                "  listuser - �г������û�\n"
                "  listroom - �г�����������\n"
                "  rmuser <userId> - ɾ��ָ���û�ID���û�\n"
                "  help - ��ʾ�˰�����Ϣ");
        }
        else {
            logger.logError("Control", "���Ϸ���ָ��: " + command);
        }
    }
    catch (const exception& e) {
        logger.logFatal("Control", "����ִ��ʧ��: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Control", "����ִ��ʱ����δ֪����");
    }
}

void command() {
    Logger& logger = Logger::getInstance();
    while (true) {
        try {
            string cmd;
            if (!getline(cin, cmd)) {
                // �����ȡʧ�ܻ�EOF���˳�ѭ��
                break;
            }
            // ÿ��������ڷ����߳��д���
            try {
                thread cmd_thread(command_runner, cmd, 0);
                cmd_thread.detach();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "���������߳�ʧ��: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "���������߳�ʱ����δ֪����");
            }
        }
        catch (const std::exception& e) {
            logger.logError("Control", "��ȡ����ʱ�����쳣: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Control", "��ȡ����ʱ����δ֪����");
        }
    }
}

void run() {
    Logger& logger = Logger::getInstance();
    try {
        // ��ʼ������������Ĭ��������
        //addroom(); // ������һ��������
        //addroom(); // �����ڶ���������
        // ����ʱ�������ã��������������ü����������ã�
        logger.logInfo("Control", "�����߳��ѿ���");
        loadConfig();
        logger.logInfo("Control", "�Ѽ��������ļ�");
        try {
            thread maint(command);
            maint.join();
        }
        catch (const std::exception& e) {
            logger.logError("Control", "���������߳�ʧ��: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Control", "���������߳�ʱ����δ֪����");
        }

        logger.logInfo("Control", "�����߳��ѽ���");
    }
    catch (const std::exception& e) {
        logger.logFatal("Control", "���п���ģ��ʱ�����쳣: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Control", "���п���ģ��ʱ����δ֪����");
    }
    return;
}
