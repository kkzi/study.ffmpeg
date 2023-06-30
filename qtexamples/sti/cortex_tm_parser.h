// Author     : Teo.Sun
// Date       : 2019-01-21
// Description: Provide a telemetry msg parser

#pragma once
#pragma warning(disable : 4251)
#pragma warning(disable : 4996)
#pragma warning(disable : 4834)

#include <functional>
#include <memory>
#include <vector>

namespace cortex
{
    /***************************************************************
     * @class cortex_tm_parser
     * @brief 该类负责从遥测消息中提取出遥测帧.
     * @note  必须保证输入的每一包数据是cortex标准的遥测消息格式.
     ***************************************************************/
    class cortex_tm_parser;
    typedef std::shared_ptr<cortex_tm_parser> cortex_tm_parser_ptr;

    typedef std::shared_ptr<std::vector<uint8_t>> data_ptr;
    typedef std::vector<uint8_t>::iterator iterator_type;

    struct tm_frame
    {
        data_ptr ptm_msg = nullptr;
        uint32_t frame_offset = 0;
        uint32_t frame_len = 0;
    };
    typedef std::shared_ptr<tm_frame> tm_frame_ptr;
    typedef std::function<void(double time, const tm_frame_ptr)> tm_frame_callback_fun_t;

    class cortex_tm_parser
    {

    public:
        /**遥测消息解析器构造函数
         * @param capacity 开辟的解析器缓存(容纳遥测消息的数量)
         */
        cortex_tm_parser(size_t capacity, int time_code);
        virtual ~cortex_tm_parser();

        void set_time_code(int time_code)
        {
            time_code_ = time_code;
        }
        bool is_running() const;
        void start();
        void stop();
        void push_tm_msg(const data_ptr ptm_msg);
        /** 设置解析出的遥测帧处理函数.*/
        void set_tm_frame_callback_fun(const tm_frame_callback_fun_t &fun);

        struct buffer_status
        {
            size_t capacity;
            size_t used;
            size_t lost;
        };
        buffer_status get_buffer_status() const;

    protected:
        virtual void parse_tm_msg(const data_ptr ptm_msg) = 0;

    protected:
        int time_code_{ 0 };
        tm_frame_callback_fun_t tm_frame_callback_fun_ = nullptr;

    private:
        struct cortex_tm_parser_imp_t;
        std::shared_ptr<cortex_tm_parser_imp_t> imp_;
    };
    /***************************************************************
     * @class hdr_tm_parser
     * @brief HDR.
     ***************************************************************/
    class hdr_tm_parser;
    typedef std::shared_ptr<hdr_tm_parser> hdr_tm_parser_ptr;

    class hdr_tm_parser : public cortex_tm_parser
    {
    public:
        hdr_tm_parser(size_t capacity, int time_code);
        virtual ~hdr_tm_parser();

    protected:
        virtual void parse_tm_msg(const data_ptr ptm_msg) override;
    };
    /***************************************************************
     * @class crt_tm_parser
     * @brief CRT.
     ***************************************************************/
    class crt_tm_parser : public cortex_tm_parser
    {
    public:
        crt_tm_parser(size_t capacity, int time_code);
        virtual ~crt_tm_parser() = default;

    protected:
        virtual void parse_tm_msg(const data_ptr ptm_msg) override;
    };
    typedef std::shared_ptr<crt_tm_parser> crt_tm_parser_ptr;
}  // namespace cortex