#include "../include/tool.h"
#include "../include/config.h"
#include "../include/datamanage.h"
#include "../include/log.h" // ������־��
#include <map>
#include <mutex>
#include <vector>
#include <tuple>
#include <sqlite3.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <algorithm>
using namespace std;
namespace manager {
    std::mutex mtx;  // ���ڱ����������ݵĻ�����
    sqlite3* db = nullptr;
    // �û����ݻ��棬��ʱ���
    struct CachedUser {
        user* userPtr;
        std::chrono::steady_clock::time_point lastAccess;
    };
    std::unordered_map<int, CachedUser> userCacheById;
    std::unordered_map<std::string, CachedUser> userCacheByName;
    // �����������
    const size_t MAX_CACHE_SIZE = 1000; // ��󻺴��û�����
    const std::chrono::minutes CACHE_EXPIRY_TIME{ 30 }; // 30���ӹ���ʱ��
    bool cacheCleanerRunning = false;
    // ��������������û�����
    void InvalidateUserCache(int uid, const std::string& name) {
        lock_guard<mutex> lock(mtx);
        auto itById = userCacheById.find(uid);
        if (itById != userCacheById.end()) {
            delete itById->second.userPtr;
            userCacheById.erase(itById);
        }
        auto itByName = userCacheByName.find(name);
        if (itByName != userCacheByName.end()) {
            // ��������ɾ������Ϊ������userCacheById��ͬ��ָ��
            userCacheByName.erase(itByName);
        }
    }
    // ���������̺߳���
    void CleanCache() {
        while (cacheCleanerRunning) {
            std::this_thread::sleep_for(std::chrono::minutes(5)); // ÿ5��������һ��
            auto now = std::chrono::steady_clock::now();
            size_t cleanedCount = 0;
            {
                lock_guard<mutex> lock(mtx);
                // ���ݹ���ʱ������
                for (auto it = userCacheById.begin(); it != userCacheById.end(); ) {
                    if (now - it->second.lastAccess > CACHE_EXPIRY_TIME) {
                        delete it->second.userPtr;
                        userCacheByName.erase(it->second.userPtr->getname());
                        it = userCacheById.erase(it);
                        cleanedCount++;
                    }
                    else {
                        ++it;
                    }
                }
                // �����Ȼ���������LRU���������ʹ�ã�����
                if (userCacheById.size() > MAX_CACHE_SIZE) {
                    // ����һ������������Ŀ�������Ա㰴������ʱ������
                    std::vector<std::pair<int, std::chrono::steady_clock::time_point>> entries;
                    for (const auto& entry : userCacheById) {
                        entries.emplace_back(entry.first, entry.second.lastAccess);
                    }
                    // ��������ʱ�����������ȣ�
                    std::sort(entries.begin(), entries.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; });
                    // ɾ����ɵ���Ŀֱ�����ǵ�������
                    for (size_t i = 0; i < entries.size() - MAX_CACHE_SIZE / 2; ++i) {
                        auto it = userCacheById.find(entries[i].first);
                        if (it != userCacheById.end()) {
                            delete it->second.userPtr;
                            userCacheByName.erase(it->second.userPtr->getname());
                            userCacheById.erase(it);
                            cleanedCount++;
                        }
                    }
                }
            }
            if (cleanedCount > 0) {
                Logger::getInstance().logInfo("cache", "������ " + std::to_string(cleanedCount) + " �����ڻ��������ʹ�õĻ�����Ŀ��");
            }
        }
    }
    // �������������߳�
    void StartCacheCleaner() {
        if (!cacheCleanerRunning) {
            cacheCleanerRunning = true;
            std::thread(CleanCache).detach();
            Logger::getInstance().logInfo("cache", "���������߳���������");
        }
    }
    // ֹͣ���������߳�
    void StopCacheCleaner() {
        cacheCleanerRunning = false;
        Logger::getInstance().logInfo("cache", "���������߳���ֹͣ��");
    }
    // ����ַ��Ƿ�ȫ
    bool SafeWord(char word) {
        if (('0' <= word && word <= '9')
            || ('a' <= word && word <= 'z')
            || ('A' <= word && word <= 'Z')
            || word == '_') return true;
        return false;
    }
    // ����û����Ƿ�Ϸ�
    bool CheckUserName(string name) {
        if (name.length() > 50) return false;
        for (int i = 0; i < name.length(); i++) {
            if (!SafeWord(name[i])) return false;
        }
        return true;
    }
    // �洢���ݵ��ļ�·��
    int usernum = 0;
    string user::getname() { return name; }
    string user::getcookie() { return cookie; }
    string user::getlabei() { return labei; }
    string user::getpassword() { return password; }
    void user::setcookie(string new_cookie) {
        lock_guard<mutex> lock(mtx);
        cookie = new_cookie;
        Keyword::replace_sql_injection_keywords(cookie);

        // �������ݿ�
        const char* updateQuery = R"(
            UPDATE users SET cookie = ? WHERE uid = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, new_cookie.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, uid);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                Logger::getInstance().logError("database", "�������ݿ��е�cookieʧ��: " + std::string(sqlite3_errmsg(db)));
            }
            sqlite3_finalize(stmt);
        }
        else {
            Logger::getInstance().logError("database", "׼�����²�ѯ���ʱ����: " + std::string(sqlite3_errmsg(db)));
        }
        // ���»����еķ���ʱ��
        auto now = std::chrono::steady_clock::now();
        auto it = userCacheById.find(uid);
        if (it != userCacheById.end()) {
            it->second.lastAccess = now;
        }
        auto itName = userCacheByName.find(name);
        if (itName != userCacheByName.end()) {
            itName->second.lastAccess = now;
        }
    }
    void user::ban() {
        lock_guard<mutex> lock(mtx);
        labei = BanedLabei;
        // �������ݿ�
        const char* updateQuery = R"(
            UPDATE users SET labei = ? WHERE uid = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, BanedLabei.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, uid);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                Logger::getInstance().logError("database", "�������ݿ��е�labeiʧ��: " + std::string(sqlite3_errmsg(db)));
            }
            sqlite3_finalize(stmt);
        }
        else {
            Logger::getInstance().logError("database", "׼�����²�ѯ���ʱ����: " + std::string(sqlite3_errmsg(db)));
        }
        // ʹ������Ч
        InvalidateUserCache(uid, name);
    }
    bool user::setname(string str) {
        if (!CheckUserName(str)) {
            return false;
        }
        name = str;
        return true;
    }
    // �����ⷵ�ص���TM int!!! int!!! int!!!
    int user::getuid() { return uid; }
    bool user::operator <(user x) {
        return uid < x.uid;
    }
    user::user(string name_, string password_, string cookie_, string labei_) {
        name = name_, password = password_, cookie = cookie_, labei = labei_;
        uid = 0;
    }
    void user::setuid(int value) {
        if (value != -1) {
            uid = value;
            usernum = max(value, usernum);
        }
        else uid = ++usernum;
    }
    // ��ʼ��SQLite3���ݿ�
    void InitDatabase(const std::string& dbPath) {
        if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("��SQLite3���ݿ�ʧ��");
        }
        const char* createTableQuery = R"(
            CREATE TABLE IF NOT EXISTS users (
                uid INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL,
                password TEXT NOT NULL,
                cookie TEXT,
                labei TEXT
            );
        )";
        char* errMsg = nullptr;
        if (sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "������ʧ��: " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
        // ��������������
        StartCacheCleaner();
    }
    // �����ݿ��������û�
    bool AddUser(const std::string& name, const std::string& psw, const std::string& cookie, const std::string& labei) {
        if (!CheckUserName(name) || Keyword::check_sql_keywords(name)) {
            Logger::getInstance().logError("database", "�û������Ϸ�: " + name);
            return false;
        }
        
        //Keyword::replace_sql_injection_keywords(name);
        
        const char* getMaxUidQuery = R"(
            SELECT IFNULL(MAX(uid), 0) FROM users;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, getMaxUidQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "��ȡ���UIDʱ��������: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        int newUid = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            newUid = sqlite3_column_int(stmt, 0) + 1;
        }
        sqlite3_finalize(stmt);
        const char* insertQuery = R"(
            INSERT INTO users (uid, name, password, cookie, labei)
            VALUES (?, ?, ?, ?, ?);
        )";
        if (sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "׼�������û�����ʱ��������: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        sqlite3_bind_int(stmt, 1, newUid);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, psw.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, cookie.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, labei.c_str(), -1, SQLITE_STATIC);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!success) {
            Logger::getInstance().logError("database", "�����û�����ʧ��: " + std::string(sqlite3_errmsg(db)));
        }
        else {
            Logger::getInstance().logInfo("database", "�û� " + name + " �ѳɹ���ӡ�");
            // �����û�����
            auto now = std::chrono::steady_clock::now();
            lock_guard<mutex> lock(mtx);
            user* newUser = new user(name, psw, cookie, labei);
            newUser->setuid(newUid);
            userCacheById[newUid] = { newUser, now };
            userCacheByName[name] = { newUser, now };
        }
        sqlite3_finalize(stmt);
        return success;
    }
    // ͨ��UID�����û�
    user* FindUser(int uid) {
        auto now = std::chrono::steady_clock::now();
        {
            lock_guard<mutex> lock(mtx);
            auto it = userCacheById.find(uid);
            if (it != userCacheById.end()) {
                it->second.lastAccess = now; // ���·���ʱ��
                return it->second.userPtr;
            }
        }
        const char* selectQuery = R"(
            SELECT name, password, cookie, labei FROM users WHERE uid = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "�����û�ʱ��������: " + std::string(sqlite3_errmsg(db)));
            return nullptr;
        }
        sqlite3_bind_int(stmt, 1, uid);
        user* foundUser = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string cookie = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            std::string labei = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            foundUser = new user(name, password, cookie, labei);
            foundUser->setuid(uid);
            // �����û�
            lock_guard<mutex> lock(mtx);
            userCacheById[uid] = { foundUser, now };
            userCacheByName[name] = { foundUser, now };
        }
        sqlite3_finalize(stmt);
        return foundUser;
    }
    // ͨ�����Ʋ����û�
    user* FindUser(string name) {
        auto now = std::chrono::steady_clock::now();
        {
            lock_guard<mutex> lock(mtx);
            auto it = userCacheByName.find(name);
            if (it != userCacheByName.end()) {
                it->second.lastAccess = now; // ���·���ʱ��
                return it->second.userPtr;
            }
        }
        if (Keyword::check_sql_keywords(name)) return nullptr;
        const char* selectQuery = R"(
            SELECT uid, password, cookie, labei FROM users WHERE name = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            return nullptr;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        user* foundUser = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int uid = sqlite3_column_int(stmt, 0);
            std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string cookie = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            std::string labei = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            foundUser = new user(name, password, cookie, labei);
            foundUser->setuid(uid);
            // �����û�
            lock_guard<mutex> lock(mtx);
            userCacheById[uid] = { foundUser, now };
            userCacheByName[name] = { foundUser, now };
        }
        sqlite3_finalize(stmt);
        return foundUser;
    }
    // �Ƴ��û�
    bool RemoveUser(int uid) {
        const char* updateQuery = R"(
            UPDATE users SET labei = ? WHERE uid = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "׼���Ƴ��û�ʱ��������: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        sqlite3_bind_text(stmt, 1, BanedLabei.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, uid);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        if (success) {
            Logger::getInstance().logInfo("database", "UIDΪ " + std::to_string(uid) + " ���û��ѱ��Ƴ���");
            // ʹ������Ч
            lock_guard<mutex> lock(mtx);
            auto it = userCacheById.find(uid);
            if (it != userCacheById.end()) {
                userCacheByName.erase(it->second.userPtr->getname());
                delete it->second.userPtr;
                userCacheById.erase(it);
            }
        }
        else {
            Logger::getInstance().logError("database", "�Ƴ��û�ʧ��: " + std::string(sqlite3_errmsg(db)));
        }
        sqlite3_finalize(stmt);
        return success;
    }
    bool checkInRoom(int number, int uid) {
        user* targetUser = FindUser(uid);
        if (targetUser == nullptr) {
            return false; // �û�δ�ҵ�
        }
        string cookie = targetUser->getcookie();
        if (cookie.empty()) {
            return false; // ��cookie
        }
        lock_guard<mutex> lock(mtx); // �����������ݷ���
        // �ָ�cookie�ַ��������ÿ������
        size_t start = 0;
        size_t end = cookie.find('&');
        while (end != string::npos) {
            string numStr = cookie.substr(start, end - start);
            int currentNum;
            if (str::safeatoi(numStr, currentNum) && currentNum == number) {
                return true;
            }
            start = end + 1;
            end = cookie.find('&', start);
        }
        // ������һ�����������һ��'&'֮��
        string lastNumStr = cookie.substr(start);
        int lastNum;
        if (str::safeatoi(lastNumStr, lastNum) && lastNum == number) {
            return true;
        }
        return false;
    }
    // ��ȡʱ�Ļ�����
    config DataFile;
    std::vector<item> list;
    // ��ӡ�쳣
    void LogError(string path, string filename, int line) {
        Logger& logger = Logger::getInstance();
        logger.logError("config", "��ȡ�û���Ϣ" + path + "/" + filename + " ��" + to_string(line) + "ʱ��������");
    }
    // ��ȡ�����ļ�
    void ReadUserData(string path, string filename) {
        // ������Ҫ����Ϊ���ݴ洢��SQLite3��
        throw std::runtime_error("������");
    }
    // ���浱ǰ�û��б�
    void WriteUserData(string path, string filename) {
        throw std::runtime_error("������");
    }
    // �������ذ����û����������UID��Ԫ������
    std::vector<std::tuple<std::string, std::string, int>> GetUserDetails() {
        const char* selectQuery = R"(
            SELECT name, password, uid FROM users;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            return {};
        }
        std::vector<std::tuple<std::string, std::string, int>> userDetails;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            int uid = sqlite3_column_int(stmt, 2);
            userDetails.emplace_back(name, password, uid);
        }
        sqlite3_finalize(stmt);
        return userDetails;
    }
    // �ر�SQLite3���ݿ⣬��ǿ��׳�Բ���¼��־
    void CloseDatabase() {
        StopCacheCleaner();
        // �������л�����û�
        {
            lock_guard<mutex> lock(mtx);
            for (auto& entry : userCacheById) {
                delete entry.second.userPtr;
            }
            userCacheById.clear();
            userCacheByName.clear();
        }
        if (db) {
            if (sqlite3_close(db) == SQLITE_OK) {
                Logger::getInstance().logInfo("database", "���ݿ��ѳɹ��رա�");
            }
            else {
                Logger::getInstance().logError("database", "�ر����ݿ�ʱ��������: " + std::string(sqlite3_errmsg(db)));
            }
            db = nullptr;
        }
        else {
            Logger::getInstance().logInfo("database", "���ݿ��ѹرջ�δ��ʼ����");
        }
    }
}