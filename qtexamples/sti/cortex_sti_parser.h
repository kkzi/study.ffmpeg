// Author     : Teo.Sun
// Date       : 2019-01-21
// Description: Provide a cortex data parser

#pragma once
#pragma warning(disable : 4244)
#pragma warning(disable : 4251)
#pragma warning(disable : 4996)

#include <functional>
#include <memory>
#include <vector>

namespace cortex
{
    /***************************************************************
     * @class cortex_sti_parser
     * @brief 该类负责遥测数据的组包和拆包, 确保输出的数据为标准的cortex遥测消息格式.
     ***************************************************************/
    typedef std::vector<uint8_t>::iterator iterator_type;
    typedef std::shared_ptr<std::vector<uint8_t>> data_ptr;
    typedef std::function<void(const data_ptr)> tm_msg_callback_fun_t;

    class cortex_sti_parser
    {
    public:
        cortex_sti_parser(size_t capacity);
        virtual ~cortex_sti_parser();

        bool is_running() const;
        void start();
        void stop();
        void push_data(iterator_type begin, iterator_type end);
        void set_tm_msg_callback_fun(const tm_msg_callback_fun_t &fun);

        struct buffer_status
        {
            size_t capacity;
            size_t used;
            size_t lost;
        };
        buffer_status get_buffer_status() const;

    private:
        struct cortex_sti_parser_imp_t;
        std::shared_ptr<cortex_sti_parser_imp_t> imp_;
    };
    typedef std::shared_ptr<cortex_sti_parser> cortex_sti_parser_ptr;
}  // namespace cortex
