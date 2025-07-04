#include "roommanager.h"  // 确保首先包含头文件
#include "../server/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "json/json.h"
#include "../data/datamanage.h"
#include "../tool/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"
#include <sqlite3.h>
#include "chatDBmanager.h"
#include <regex>

// 懒加载监控线程控制
std::atomic<bool> monitorThreadRunning(false);
std::mutex monitorThreadMutex;
std::thread monitorThread;

// Ensure room initialization logic is robust
vector<chatroom> room(MAXROOM);
bool used[MAXROOM] = { false };

// 全局缓存，存储非隐藏聊天室的列表
struct RoomCacheEntry {
    int id;
    std::string name;
};
std::vector<RoomCacheEntry> roomListCache;
bool roomCacheValid = false;
std::mutex roomCacheMutex;

// 懒加载相关函数实现

// 启动聊天室监控线程
void startRoomMonitor() {
    std::lock_guard<std::mutex> lock(monitorThreadMutex);
    
    if (!monitorThreadRunning) {
        monitorThreadRunning = true;
        monitorThread = std::thread(monitorRooms);
        monitorThread.detach();
        
        Logger& logger = Logger::getInstance();
        logger.logInfo("roommanager", "聊天室监控线程已启动");
    }
}

// 激活指定聊天室
bool activateRoom(int roomId) {
    Logger& logger = Logger::getInstance();
    
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        logger.logWarning("roommanager", "尝试激活不存在的聊天室 ID: " + std::to_string(roomId));
        return false;
    }
    
    if (!room[roomId].isActivated()) {
        logger.logInfo("roommanager", "激活聊天室 ID: " + std::to_string(roomId));
        return room[roomId].start();
    } else {
        // 如果已经激活，只更新访问时间
        room[roomId].updateAccessTime();
        return true;
    }
}

// 检查并卸载不活跃的聊天室
void checkInactiveRooms() {
    Logger& logger = Logger::getInstance();
    time_t currentTime = time(nullptr);
    int deactivatedCount = 0;
    
    for (int i = 1; i < MAXROOM; i++) {
        if (used[i] && room[i].isActivated()) {
            time_t lastAccess = room[i].getLastAccessTime();
            if (currentTime - lastAccess > ROOM_INACTIVITY_TIMEOUT) {
                if (room[i].deactivate()) {
                    deactivatedCount++;
                    logger.logInfo("roommanager", "已卸载闲置聊天室 ID: " + std::to_string(i) + 
                                  "，闲置时间: " + std::to_string(currentTime - lastAccess) + " 秒");
                }
            }
        }
    }
    
    if (deactivatedCount > 0) {
        logger.logInfo("roommanager", "本次检查共卸载 " + std::to_string(deactivatedCount) + " 个闲置聊天室");
    }
}

// 聊天室监控线程主函数
void monitorRooms() {
    Logger& logger = Logger::getInstance();
    logger.logInfo("roommanager", "聊天室监控线程开始运行");
    
    while (monitorThreadRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(ROOM_CHECK_INTERVAL));
        checkInactiveRooms();
    }
    
    logger.logInfo("roommanager", "聊天室监控线程已停止");
}

// 在添加、删除或修改聊天室时调用此函数使缓存失效
void invalidateRoomCache() {
    std::lock_guard<std::mutex> lock(roomCacheMutex);
    roomCacheValid = false;
}

// 更新聊天室列表缓存
void updateRoomListCache() {
    Logger& logger = Logger::getInstance();
    logger.logInfo("roommanager", "更新聊天室列表缓存");
    
    std::lock_guard<std::mutex> lock(roomCacheMutex);
    roomListCache.clear();
    
    try {
        for (int i = 1; i < MAXROOM; i++) {
            if (used[i] && !room[i].hasFlag(chatroom::RoomFlags::ROOM_HIDDEN)) {
                // 即使聊天室未加载，也可以安全地获取标题
                roomListCache.push_back({i, room[i].gettittle()});
            }
        }

        roomCacheValid = true;
        logger.logInfo("roommanager", "聊天室列表缓存更新完成，共 " + std::to_string(roomListCache.size()) + " 个聊天室");
    } catch (const std::exception& e) {
        logger.logError("roommanager", "更新聊天室列表缓存时出错: " + std::string(e.what()));
        // 发生错误时，确保缓存标记为无效
        roomCacheValid = false;
    }
}

// 从数据库加载聊天室信息
void loadChatroomsFromDB() {
    Logger& logger = Logger::getInstance();
    ChatDBManager& dbManager = ChatDBManager::getInstance();

    logger.logInfo("roommanager", "开始从数据库加载聊天室...");

    std::vector<int> roomIds = dbManager.getAllChatRoomIds();
    logger.logInfo("roommanager", "找到 " + std::to_string(roomIds.size()) + " 个聊天室记录");

    int roomcount = 0;

    for (int id : roomIds) {
        if (id >= 1 && id < MAXROOM) {
            std::string title, passwordHash, password;
            unsigned int flags;

            if (dbManager.getChatRoom(id, title, passwordHash, password, flags)) {
                if (!used[id]) {
                    used[id] = true;
                    room[id].init();
                    room[id].setRoomID(id);
                    room[id].settittle(title);
                    room[id].setPasswordHash(passwordHash);
                    room[id].setPassword(password);
                    room[id].setflag(flags);

                    // 不再立即启动聊天室
                    // 仅初始化元数据
                    //logger.logInfo("roommanager", "已从数据库加载聊天室元数据 ID: " + std::to_string(id) + ", 标题: " + title);
                    roomcount++;
                }
            } else {
                logger.logWarning("roommanager", "无法从数据库获取聊天室信息，ID: " + std::to_string(id));
            }
        }
    }

    logger.logInfo("roommanager", "聊天室元数据加载完成, 共加载 " + std::to_string(roomcount) + " 个聊天室");
}

int addroom() {
    ChatDBManager& dbManager = ChatDBManager::getInstance();

    for (int i = 1; i < MAXROOM; i++) {
        if (!used[i]) {
            used[i] = true;
            room[i].init();
            room[i].setRoomID(i);

            // 将新聊天室添加到数据库
            dbManager.createChatRoom(i, "", "", "", 0);

            // 使缓存失效
            invalidateRoomCache();

            return i;
        }
    }
    return -1;
}

void delroom(int x) {
    if (x >= 1 && x < MAXROOM && used[x]) {
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.deleteChatRoom(x);

        // 确保聊天室被卸载
        if (room[x].isActivated()) {
            room[x].deactivate();
        }

        used[x] = false;
        room[x].init();

        // 使缓存失效
        invalidateRoomCache();
    }
    else {
        Logger& logger = Logger::getInstance();
        logger.logWarning("chatroom::roomManager", "未能删除");
    }
}

int editroom(int x, string Roomtittle) {
    if (x >= room.size()) return 1;
    room[x].settittle(Roomtittle);

    // 更新数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::string title = room[x].gettittle();
    std::string passwordHash = room[x].getPasswordHash();
    std::string password = room[x].GetPassword();
    unsigned int flags = 0;
    if (room[x].hasFlag(chatroom::ROOM_HIDDEN)) flags |= chatroom::ROOM_HIDDEN;
    if (room[x].hasFlag(chatroom::ROOM_NO_JOIN)) flags |= chatroom::ROOM_NO_JOIN;

    dbManager.updateChatRoom(x, title, passwordHash, password, flags);

    // 使缓存失效
    invalidateRoomCache();

    return 0;
}

int setroomtype(int x, int type) {
    if (x >= room.size()) return 1;
    room[x].setflag(type);

    // 更新数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::string title = room[x].gettittle();
    std::string passwordHash = room[x].getPasswordHash();
    std::string password = room[x].GetPassword();
    unsigned int flags = type; // 直接使用传入的flags

    dbManager.updateChatRoom(x, title, passwordHash, password, flags);

    // 使缓存失效
    invalidateRoomCache();

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
    // 获取 Cookie 并解析
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);

    // If uid is still empty and cookies is non-empty, try to use cookies as uid.
    if (uid.empty() && !cookies.empty() && cookies.find('=') == std::string::npos) {
        uid = cookies;
    }

    // 转换 uid 为整数
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        // 修改：返回空数组而不是400错误
        res.status = 200;
        res.set_content("[]", "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        return;
    }

    // 查找用户
    manager::user* user = manager::FindUser(uid_);
    if (user == nullptr) {
        // 修改：返回空数组而不是404错误
        res.status = 200;
        res.set_content("[]", "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        return;
    }

    // 获取用户的聊天室 ID 列表
    std::string List = user->getcookie();
    if (List.empty()) {
        res.status = 200; // OK
        res.set_content("[]", "application/json"); // 返回空的 JSON 数组
        return;
    }

    // 分割 List 字符串，提取每个聊天室 ID
    std::vector<std::string> roomIds;
    std::stringstream ss(List);
    std::string roomId;
    while (std::getline(ss, roomId, '&')) {
        roomIds.push_back(roomId);
    }

    // 创建一个 JSON 响应对象
    Json::Value response(Json::arrayValue);

    // 遍历所有聊天室 ID，获取对应的聊天室名称
    for (const std::string& roomId : roomIds) {
        int room_id;
        if (!str::safeatoi(roomId, room_id)) {
            continue; // 跳过无效的聊天室 ID
        }

        std::string roomName = getRoomName(room_id);

        // 构造每个聊天室的 JSON 对象
        Json::Value room;
        room["id"] = roomId;  // 添加聊天室的 ID
        room["name"] = roomName;  // 添加聊天室的名称

        response.append(room);  // 将聊天室对象添加到 JSON 数组中
    }

    // 设置响应头，支持跨域
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");

    // 设置响应内容为 JSON 格式的字符串
    res.set_content(response.toStyledString(), "application/json");
}

void getAllList(const httplib::Request& req, httplib::Response& res) {
    // 获取分页参数
    int page = 0;
    int pageSize = 50; // 默认每页50个聊天室

    if (req.has_param("page")) {
        try {
            page = std::stoi(req.get_param_value("page"));
            if (page < 0) page = 0;
        } catch (...) {
            // 忽略无效参数
        }
    }

    if (req.has_param("size")) {
        try {
            pageSize = std::stoi(req.get_param_value("size"));
            if (pageSize <= 0) pageSize = 50;
            if (pageSize > 200) pageSize = 200; // 限制最大页面大小
        } catch (...) {
            // 忽略无效参数
        }
    }

    // 如果缓存无效，更新缓存
    {
        std::lock_guard<std::mutex> lock(roomCacheMutex);
        if (!roomCacheValid) {
            roomCacheMutex.unlock();
            updateRoomListCache();
            roomCacheMutex.lock();
        }
    }

    // 计算分页数据
    int startIndex = page * pageSize;
    int endIndex = startIndex + pageSize;

    // 预分配响应对象的大小，避免动态扩容
    Json::Value response(Json::arrayValue);

    // 从缓存中读取数据
    {
        std::lock_guard<std::mutex> lock(roomCacheMutex);

        // 检查范围是否有效
        if (startIndex < roomListCache.size()) {
            // 确保结束索引不超出边界
            if (endIndex > roomListCache.size()) {
                endIndex = roomListCache.size();
            }

            // 从缓存中添加指定范围的聊天室
            for (int i = startIndex; i < endIndex; i++) {
                Json::Value roomObj;
                roomObj["id"] = std::to_string(roomListCache[i].id);
                roomObj["name"] = roomListCache[i].name;
                response.append(roomObj);
            }
        }
    }

    // 添加分页信息到响应头
    res.set_header("X-Total-Count", std::to_string(roomListCache.size()));
    res.set_header("X-Page", std::to_string(page));
    res.set_header("X-Page-Size", std::to_string(pageSize));

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Expose-Headers", "X-Total-Count, X-Page, X-Page-Size");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}
//
// enum class RoomResult {
//     Success,
//     UserNotFound,
//     RoomNotFound,
//     PasswordMismatch,
//     RoomAlreadyAdded,
//     RoomNotJoined,
//     RoomUnableJoin
// };
//
// enum class RoomOperation {
//     JOIN,
//     QUIT
// };

RoomResult AddRoomToUser(int uid, int roomId, const std::string& passwordHash) {
    // Check if the user exists
    manager::user* user = manager::FindUser(uid);
    if (user == nullptr) {
        return RoomResult::UserNotFound;
    }

    // Check if the room exists and is in use
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        return RoomResult::RoomNotFound;
    }

    // Check if the password hash matches
    if (!room[roomId].getPasswordHash().empty() && room[roomId].getPasswordHash() != passwordHash) {
        return RoomResult::PasswordMismatch;
    }
    // 跳过隐藏的聊天室
    if (room[roomId].hasFlag(chatroom::RoomFlags::ROOM_NO_JOIN)) {
        return RoomResult::RoomUnableJoin;
    }
    // Get the current cookie string
    std::string cookie = user->getcookie();
    std::string roomIdStr = std::to_string(roomId);

    // Check if the room is already added
    std::stringstream ss(cookie);
    std::string token;
    while (std::getline(ss, token, '&')) {
        if (token == roomIdStr) {
            return RoomResult::RoomAlreadyAdded;
        }
    }

    // Add the room to the user's cookie
    if (!cookie.empty()) {
        cookie += "&";
    }
    cookie += roomIdStr;
    user->setcookie(cookie);

    return RoomResult::Success;
}

RoomResult QuitRoomToUser(int uid, int roomId) {
    // Check if the user exists
    manager::user* user = manager::FindUser(uid);
    if (user == nullptr) {
        return RoomResult::UserNotFound;
    }

    // Check if the room exists
    if (roomId < 1 || roomId >= MAXROOM) {
        return RoomResult::RoomNotFound;
    }

    // Get the current cookie string
    std::string cookie = user->getcookie();
    std::string roomIdStr = std::to_string(roomId);

    // Split the cookie string and rebuild without the target room
    std::stringstream ss(cookie);
    std::string token;
    std::string newCookie;
    bool found = false;

    while (std::getline(ss, token, '&')) {
        if (token == roomIdStr) {
            found = true;
            continue;
        }
        if (!newCookie.empty()) {
            newCookie += "&";
        }
        newCookie += token;
    }

    if (!found) {
        return RoomResult::RoomNotJoined;
    }

    user->setcookie(newCookie);
    return RoomResult::Success;
}

bool verifyCookiePassword(int uid, string password) {
    Logger& logger = logger.getInstance();
    manager::user nowuser = *manager::FindUser(uid);
    if (nowuser.getpassword() != password) {
        return false;
    }
    return true;
}

void editRoomToUserRoute(const httplib::Request& req, httplib::Response& res) {
    // Parse the user ID from the cookie
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        res.status = 400;
        res.set_content("Invalid UID format", "text/plain");
        return;
    }

    if (!verifyCookiePassword(uid_, password)) {
        res.status = 403;
        res.set_content("Invalid cookie authentication", "text/plain");
        return;
    }

    // Parse the room ID from the request parameters
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

    // Parse operation type
    if (!req.has_param("operation")) {
        res.status = 400;
        res.set_content("Missing operation parameter", "text/plain");
        return;
    }
    std::string op = req.get_param_value("operation");
    RoomOperation operation = (op == "join") ? RoomOperation::JOIN : RoomOperation::QUIT;

    RoomResult result;
    if (operation == RoomOperation::JOIN) {
        std::string passwordHash = req.has_param("passwordHash") ? req.get_param_value("passwordHash") : "";
        result = AddRoomToUser(uid_, roomId, passwordHash);
    }
    else {
        result = QuitRoomToUser(uid_, roomId);
    }
    Logger& logger = logger.getInstance();
    // Handle the result
    switch (result) {
    case RoomResult::Success:
        res.status = 200;
        res.set_content(operation == RoomOperation::JOIN ? "Joined successfully" : "Quit successfully", "text/plain");
        logger.logInfo("RoomManager", "用户 " + std::to_string(uid_) + " " + (operation == RoomOperation::JOIN ? "joined" : "quit") + " room " + std::to_string(roomId));
        break;
    case RoomResult::UserNotFound:
        res.status = 404;
        res.set_content("User not found", "text/plain");
        logger.logWarning("RoomManager", "用户 " + std::to_string(uid_) + " not found");
        break;
    case RoomResult::RoomNotFound:
        res.status = 404;
        res.set_content("Room not found", "text/plain");
        logger.logWarning("RoomManager", "Room " + std::to_string(roomId) + " not found");
        break;
    case RoomResult::PasswordMismatch:
        res.status = 403;
        res.set_content("Password mismatch", "text/plain");
        logger.logWarning("RoomManager", "密码错误 " + std::to_string(uid_) + " in room " + std::to_string(roomId));
        break;
    case RoomResult::RoomAlreadyAdded:
        res.status = 409;
        res.set_content("Room already added", "text/plain");
        break;
    case RoomResult::RoomNotJoined:
        res.status = 404;
        res.set_content("Room not joined", "text/plain");
        break;
    case RoomResult::RoomUnableJoin:
        res.status = 403;
		res.set_content("Room Unable to Join", "text/plain");
    }
}

std::vector<std::tuple<int, std::string, std::string>> GetRoomListDetails() {
    std::vector<std::tuple<int, std::string, std::string>> roomDetails;

    // Iterate through all rooms
    for (int i = 1; i < MAXROOM; i++) {
        if (used[i]) {
            // Get room name and password
            std::string name = room[i].gettittle();
            std::string password = room[i].GetPassword();

            // Add tuple to vector
            roomDetails.emplace_back(i, name, password);
        }
    }

    return roomDetails;
}

void getChatroomName(const httplib::Request& req, httplib::Response& res) {
    // Validate if the roomId parameter exists
    if (!req.has_param("roomId")) {
        res.status = 400; // Bad Request
        res.set_content("Missing roomId parameter", "text/plain");
        return;
    }

    // Parse the roomId parameter
    std::string roomIdStr = req.get_param_value("roomId");
    int roomId;
    if (!str::safeatoi(roomIdStr, roomId)) {
        res.status = 400; // Bad Request
        res.set_content("Invalid roomId format", "text/plain");
        return;
    }

    // Check if the roomId is valid and in use
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        res.status = 404; // Not Found
        res.set_content("Room not found", "text/plain");
        return;
    }

    // Retrieve the room name
    std::string roomName = room[roomId].gettittle();

    // Respond with the room name
    Json::Value response;
    response["id"] = roomIdStr;
    response["name"] = roomName;

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}

void start_manager() {
    Logger& logger = Logger::getInstance();

    try {
        // 从数据库加载聊天室
        logger.logInfo("roommanager", "开始初始化聊天室管理器...");
        loadChatroomsFromDB();

        // 检查并确保聊天室 id=1 存在（用于私聊功能）
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        std::string title, passwordHash, password;
        unsigned int flags;
        
        if (!dbManager.getChatRoom(1, title, passwordHash, password, flags)) {
            // 如果聊天室 id=1 不存在，则创建它
            logger.logInfo("roommanager", "创建聊天室 id=1 (用于私聊功能)");
            
            // 设置聊天室属性
            if (!used[1]) {
                used[1] = true;
                room[1].init();
                room[1].setRoomID(1);
                room[1].settittle("私聊");
                // 不设置密码，保持为空字符串
                room[1].setflag(0); // 不设置任何特殊标志
                
                // 将新聊天室添加到数据库
                dbManager.createChatRoom(1, "私聊", "", "", 0);
                
                logger.logInfo("roommanager", "聊天室 id=1 创建成功");
            }
        } else {
            logger.logInfo("roommanager", "聊天室 id=1 已存在，用于私聊功能");
            // 确保本地状态正确设置
            if (!used[1]) {
                used[1] = true;
                room[1].init();
                room[1].setRoomID(1);
                room[1].settittle(title);
                room[1].setPasswordHash(passwordHash);
                room[1].setPassword(password);
                room[1].setflag(flags);
            }
        }

        // 启动聊天室监控线程
        startRoomMonitor();

        // 启动私聊系统
        PrivateChat& privateChat = PrivateChat::getInstance();
        if (!privateChat.start()) {
            logger.logError("roommanager", "无法启动私聊系统");
        } else {
            logger.logInfo("roommanager", "私聊系统启动成功");
        }
    } catch (const std::exception& e) {
        logger.logError("roommanager", "加载聊天室���发生异常: " + std::string(e.what()));
    } catch (...) {
        logger.logError("roommanager", "加载聊天室时发生未知异常");
    }

    Server& server = Server::getInstance(HOST);

    // 添加一个拦截器路由，用于懒加载聊天室 - 直接访问 /chatX 格式
    server.getInstance().handleRequest(R"(/chat(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        // 从URL提取聊天室ID
        std::string roomIdStr = req.matches[1].str();
        int roomId;
        if (!str::safeatoi(roomIdStr, roomId)) {
            res.status = 400;
            res.set_content("Invalid room ID", "text/plain");
            return;
        }

        // 检查聊天室是否存在
        if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
            res.status = 404;
            res.set_content("Room not found", "text/plain");
            return;
        }

        // 激活聊天室
        if (!activateRoom(roomId)) {
            res.status = 500;
            res.set_content("Failed to activate room", "text/plain");
            return;
        }

        // 特殊处理：对于chat1提供privatechat.html而不是index.html
        if (roomId == 1) {
            std::ifstream htmlFile("html/files/privatechat.html");
            if (htmlFile) {
                std::stringstream buffer;
                buffer << htmlFile.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.status = 404;
                res.set_content("Private chat room template not found", "text/plain");
            }
        } else {
            // 对于其他聊天室，提供标准HTML页面
            std::ifstream htmlFile("html/index.html");
            if (htmlFile) {
                std::stringstream buffer;
                buffer << htmlFile.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.status = 404;
                res.set_content("Chat room template not found", "text/plain");
            }
        }
    });

    // 懒加载拦截器 - 标准格式 /chat/X
    server.getInstance().handleRequest(R"(/chat/(\d+)$)", [](const httplib::Request& req, httplib::Response& res) {
        // 从URL提取聊天室ID
        std::string roomIdStr = req.matches[1].str();
        int roomId;
        if (!str::safeatoi(roomIdStr, roomId)) {
            res.status = 400;
            res.set_content("Invalid room ID", "text/plain");
            return;
        }

        // 检查聊天室是否存在
        if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
            res.status = 404;
            res.set_content("Room not found", "text/plain");
            return;
        }

        // 激活聊天室
        if (!activateRoom(roomId)) {
            res.status = 500;
            res.set_content("Failed to activate room", "text/plain");
            return;
        }

        // 特殊处理：对于chat1提供privatechat.html而不是index.html
        if (roomId == 1) {
            std::ifstream htmlFile("html/privatechat.html");
            if (htmlFile) {
                std::stringstream buffer;
                buffer << htmlFile.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.status = 404;
                res.set_content("Private chat room template not found", "text/plain");
            }
        } else {
            // 对于其他聊天室，提供标准HTML页面
            std::ifstream htmlFile("html/index.html");
            if (htmlFile) {
                std::stringstream buffer;
                buffer << htmlFile.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.status = 404;
                res.set_content("Chat room template not found", "text/plain");
            }
        }
    });

    // 修改消息路由拦截器，确保聊天室被访问时激活
    server.getInstance().handleRequest(R"(/chat/(\d+)/messages)", [](const httplib::Request& req, httplib::Response& res) {
        // 从URL提取聊天室ID
        std::string roomIdStr = req.matches[1].str();
        int roomId;
        if (!str::safeatoi(roomIdStr, roomId)) {
            res.status = 400;
            res.set_content("Invalid room ID", "text/plain");
            return;
        }

        // 确保聊天室已激活
        if (!activateRoom(roomId)) {
            res.status = 500;
            res.set_content("Failed to activate room", "text/plain");
            return;
        }

        // 将请求转发给原始路由处理程序
        room[roomId].getChatMessages(req, res);
    });

    // 全部消息路由
    server.getInstance().handleRequest(R"(/chat/(\d+)/all-messages)", [](const httplib::Request& req, httplib::Response& res) {
        // 从URL提取聊天室ID
        std::string roomIdStr = req.matches[1].str();
        int roomId;
        if (!str::safeatoi(roomIdStr, roomId)) {
            res.status = 400;
            res.set_content("Invalid room ID", "text/plain");
            return;
        }

        // 确保聊天室已激活
        if (!activateRoom(roomId)) {
            res.status = 500;
            res.set_content("Failed to activate room", "text/plain");
            return;
        }

        // 将请求转发给原始路由处理程序
        room[roomId].getAllChatMessages(req, res);
    });

    // 图片上传路由
    server.getInstance().handleRequest(R"(/chat/(\d+)/upload)", [](const httplib::Request& req, httplib::Response& res) {
        // 从URL提取聊天室ID
        std::string roomIdStr = req.matches[1].str();
        int roomId;
        if (!str::safeatoi(roomIdStr, roomId)) {
            res.status = 400;
            res.set_content("Invalid room ID", "text/plain");
            return;
        }

        // 确保聊天室已激活
        if (!activateRoom(roomId)) {
            res.status = 500;
            res.set_content("Failed to activate room", "text/plain");
            return;
        }

        // 将请求转发给原始路由处理程序
        room[roomId].uploadImage(req, res);
    });

    // 添加其他路由处理...
    server.handleRequest("/list", getRoomList);
    server.handleRequest("/allchatlist", getAllList);
    server.handleRequest("/joinquitroom", editRoomToUserRoute);
    server.handleRequest("/chatroomname", getChatroomName);

    // 添加获���用户名的路由 - 修复404错误
    server.handleRequest("/user/username", manager::getUsername);

    // 添加获取用户列表的路由
    server.handleRequest("/api/users", manager::getUserList);

    // 提供图片文件 /logo.png
    server.getInstance().handleRequest("/logo.png", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream logoFile("html/logo.png", std::ios::binary);
        if (logoFile) {
            std::stringstream buffer;
            buffer << logoFile.rdbuf();
            res.set_content(buffer.str(), "image/png");
        }
        else {
            res.status = 404;
            res.set_content("Logo not found", "text/plain");
        }
    });

    // 提供 JS 文件 /chat/js
    server.getInstance().handleRequest("/chat/js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/chatroom.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=gbk"); // 修改编码
        }
        else {
            res.status = 404;
            res.set_content("chatroom.js not found", "text/plain");
        }
    });

    // 提供 JS 文件 /chatlist/js
    server.getInstance().handleRequest("/chatlist/js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/chatlist.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=gbk"); // 修改编码
        }
        else {
            res.status = 404;
            res.set_content("chatlist.js not found", "text/plain");
        }
    });

    // 添加 chatlist.html 的路由
    server.getInstance().serveFile("/chatlist", "html/chatlist.html"); // 确保路径正确

    // 提供图片文件 /images/*，动态路由
    server.getInstance().handleRequest(R"(/images/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string imagePath = "html/images/" + req.matches[1].str();  // 获取图片文件名
        std::ifstream imageFile(imagePath, std::ios::binary);

        if (imageFile) {
            std::stringstream buffer;
            buffer << imageFile.rdbuf();

            // 自动推测图片的 MIME 类型
            std::string extension = imagePath.substr(imagePath.find_last_of('.') + 1);
            std::string mimeType = "images/" + extension;

            res.set_content(buffer.str(), mimeType);
        }
        else {
            res.status = 404;
            res.set_content("Image not found", "text/plain");
        }
    });

    // 提供文件 /files/*，动态路由
    server.getInstance().handleRequest(R"(/files/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string filePath = "html/files/" + req.matches[1].str();  // 获取文件名
        std::ifstream file(filePath, std::ios::binary);

        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();

            // 自动推测文件的 MIME 类型
            std::string mimeType = Server::detectMimeType(filePath);

            res.set_content(buffer.str(), mimeType);
        }
        else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
    });
    
    // 初始化聊天室列表缓存
    updateRoomListCache();

    // 提供私聊页面的JS文件
    server.getInstance().handleRequest("/files/privatechat.html.js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/files/privatechat.html.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content("privatechat.html.js not found", "text/plain");
        }
    });
}