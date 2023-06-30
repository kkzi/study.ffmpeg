#pragma once
#pragma warning(disable : 4251)
#pragma warning(disable : 4834)

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class sync_tcp_client;
typedef std::shared_ptr<sync_tcp_client> sync_tcp_client_ptr;

class sync_tcp_client
{

public:
    sync_tcp_client();
    ~sync_tcp_client();

    /** 绑定客户端使用的网卡ip和端口号.
     * @param ip	  客户端网卡
     * @param port 客户端使用端口
     * @return false:绑定失败，true:绑定成功
     * @note 如果不掉用该接口绑定，会自动使用可用的网卡和端口
     */
    bool bind(const std::string &ip, uint16_t port = 0);

    /** 判断连接状态.
     * @return true  已连接
     * @return false 未连接
     */
    bool is_connected();

    /** 连接服务端.
     * @param ip	  服务端地址
     * @param port 服务端端口
     */
    bool connect(const std::string &ip, uint16_t port);

    /** 断开连接 */
    void disconnect();

    /** 发送消息.
     * @param buf 发送消息的缓冲区
     * @return 发送成功返回发送的字节数，失败返回0
     */
    int send(const std::vector<int32_t> &buf);
    int send(const std::vector<uint8_t> &buf);

    /** 接收消息.
     * @param buf 接收消息的缓冲区
     * @return 接收成功返回收到的字节数，失败返回0
     */
    int receive_some(std::vector<uint8_t> &buf);

    /** 接收消息.
     * @param buf  接收消息的缓冲区
     * @param size 接收的大小
     * @return 接收成功返回size大小，失败返回0
     */
    int receive(std::vector<uint8_t> &buf, size_t size);

private:
    struct sync_tcp_client_imp_t;
    std::shared_ptr<sync_tcp_client_imp_t> imp_;
};