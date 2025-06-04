#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H
//�������byte��ͻ�����һ��include˳��
#include "../include/httplib.h"
#include <string>
#include <functional>
#include <unordered_set>  // ��� unordered_set�����ڴ洢������� IP
#include "../json/json.h"  // ���� json ��
#include <memory>  // ��� memory����������ָ��

extern std::string HOST;
extern int PORT;
const std::string ROOTURL = "/chat";
class Server {
public:
    // ��ȡ Server ʵ����ʹ�õ���ģʽ
    static Server& getInstance(const std::string& host = HOST, int port = PORT);

    // Ϊָ���� endpoint �ṩ HTML ������Ӧ
    void serveHtml(const std::string& endpoint, const std::string& htmlContent);

    // Ϊָ���� endpoint �ṩ�ļ���Ӧ
    void serveFile(const std::string& endpoint, const std::string& filePath);

    // ���ô��� GET ����ķ���
    void handleRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& handler);

    // ���ô��� POST ����ķ���
    void handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&, const Json::Value&)>& handler);
    void handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& fileHandler);

    // ����������
    void start();

    // ������Ӧ Cookie
    void setCookie(httplib::Response& res, const std::string& cookieName, const std::string& cookieValue);

    // ���� JSON ������Ϊ��Ӧ
    void sendJson(const Json::Value& jsonResponse, httplib::Response& res);

    // ��ȡ�����ÿ�������Ĺ���
    void setCorsHeaders(httplib::Response& res);

    // �����û���¼ƾ��
    void setToken(httplib::Response& res, const std::string& uid, const std::string& clientid);

    // ��ӷ�� IP �ķ���
    void banIP(const std::string& ipAddress);

    void debanIP(const std::string& ipAddress);

    void setHOST(std::string x);
    void setPORT(int x);
    std::string getHOST();
    int getPORT();

    // �������� SSL ƾ�ݵķ���
    void setSSLCredentials(const std::string& cert, const std::string& key);

    // �����ļ���׺�Զ��Ʋ� MIME ����
    static std::string detectMimeType(const std::string& filePath);
private:
    Server(const std::string& host, int port);
    ~Server() = default;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    std::string server_host;
    int server_port;

    // �滻Ϊ SSLServer ������ָ��
    std::unique_ptr<httplib::Server> server;


    // �����洢֤�����Կ�ļ����ĳ�Ա����
    std::string cert_file;
    std::string key_file;

    
    

    // �洢������� IP ��ַ
    std::unordered_set<std::string> bannedIPs;

    // �ж� IP �Ƿ񱻷��
    bool isBanned(const std::string& ipAddress) const;
};

#endif