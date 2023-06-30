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

    /** �󶨿ͻ���ʹ�õ�����ip�Ͷ˿ں�.
     * @param ip	  �ͻ�������
     * @param port �ͻ���ʹ�ö˿�
     * @return false:��ʧ�ܣ�true:�󶨳ɹ�
     * @note ��������øýӿڰ󶨣����Զ�ʹ�ÿ��õ������Ͷ˿�
     */
    bool bind(const std::string &ip, uint16_t port = 0);

    /** �ж�����״̬.
     * @return true  ������
     * @return false δ����
     */
    bool is_connected();

    /** ���ӷ����.
     * @param ip	  ����˵�ַ
     * @param port ����˶˿�
     */
    bool connect(const std::string &ip, uint16_t port);

    /** �Ͽ����� */
    void disconnect();

    /** ������Ϣ.
     * @param buf ������Ϣ�Ļ�����
     * @return ���ͳɹ����ط��͵��ֽ�����ʧ�ܷ���0
     */
    int send(const std::vector<int32_t> &buf);
    int send(const std::vector<uint8_t> &buf);

    /** ������Ϣ.
     * @param buf ������Ϣ�Ļ�����
     * @return ���ճɹ������յ����ֽ�����ʧ�ܷ���0
     */
    int receive_some(std::vector<uint8_t> &buf);

    /** ������Ϣ.
     * @param buf  ������Ϣ�Ļ�����
     * @param size ���յĴ�С
     * @return ���ճɹ�����size��С��ʧ�ܷ���0
     */
    int receive(std::vector<uint8_t> &buf, size_t size);

private:
    struct sync_tcp_client_imp_t;
    std::shared_ptr<sync_tcp_client_imp_t> imp_;
};