#include "cortex_sti_parser.h"
#include "lockfree_spsc_queue.h"
#include <iostream>

const static int32_t CORTEX_MSG_HEADER = 1234567890;   // Cortex 数据包头
const static int32_t CORTEX_MSG_TAILER = -1234567890;  // Cortex 数据包尾

namespace cortex
{
    struct cortex_sti_parser::cortex_sti_parser_imp_t
    {
        cortex_sti_parser_imp_t(size_t capacity)
            : data_buf_(capacity)
        {
        }

        std::atomic_bool is_running_ = false;
        std::atomic_uint64_t lost_count_ = 0;
        lockfree_spsc_queue<uint8_t> data_buf_;
        std::vector<uint8_t> parse_buf_;
        std::thread parse_thread_;
        tm_msg_callback_fun_t tm_msg_callback_fun_ = nullptr;
    };

    cortex_sti_parser::cortex_sti_parser(size_t capacity)
        : imp_(new cortex_sti_parser_imp_t(capacity))
    {
    }

    cortex_sti_parser::~cortex_sti_parser()
    {
        stop();
    }

    bool cortex_sti_parser::is_running() const
    {
        return imp_->is_running_;
    }

    void cortex_sti_parser::start()
    {
        if (!imp_->is_running_)
        {
            imp_->is_running_ = true;
            imp_->lost_count_ = 0;

            imp_->parse_thread_ = std::thread([=]() {
                //初始化Cortex消息头和尾
                std::vector<uint8_t> msg_head(sizeof(int32_t));
                memcpy(msg_head.data(), &CORTEX_MSG_HEADER, sizeof(CORTEX_MSG_HEADER));
                std::reverse(msg_head.begin(), msg_head.end());

                std::vector<uint8_t> msg_tail(sizeof(int32_t));
                memcpy(msg_tail.data(), &CORTEX_MSG_TAILER, sizeof(CORTEX_MSG_TAILER));
                std::reverse(msg_tail.begin(), msg_tail.end());

                try
                {
                    std::vector<uint8_t> vec_4bytes(4, 0x0);
                    while (imp_->is_running_)
                    {
                        auto poped_data = imp_->data_buf_.pop();
                        //弹出缓存所有数据到解析器缓存
                        auto existed_size = imp_->parse_buf_.size();
                        imp_->parse_buf_.resize(existed_size + poped_data->size());
                        std::copy(poped_data->begin(), poped_data->end(), imp_->parse_buf_.begin() + existed_size);

                        //搜索Cortex消息头
                        auto head = std::search(imp_->parse_buf_.begin(), imp_->parse_buf_.end(), msg_head.begin(), msg_head.end());
                        auto index = 0, len = 0, tail = 0;
                        while (head != imp_->parse_buf_.end())
                        {
                            index = head - imp_->parse_buf_.begin();
                            if (index + sizeof(int32_t) * 2 < imp_->parse_buf_.size())
                            {
                                //根据STI获取数据长度
                                std::copy(head + sizeof(int32_t), head + sizeof(int32_t) * 2, vec_4bytes.begin());
                                std::reverse(vec_4bytes.begin(), vec_4bytes.end());
                                memcpy(&len, vec_4bytes.data(), sizeof(int32_t));
                                //判断缓存内是否包含完整该包数据
                                if (index + len <= imp_->parse_buf_.size())
                                {
                                    //根据STI获取TCP-tail
                                    std::copy(head + len - sizeof(int32_t), head + len, vec_4bytes.begin());
                                    std::reverse(vec_4bytes.begin(), vec_4bytes.end());
                                    memcpy(&tail, vec_4bytes.data(), sizeof(int32_t));
                                    if (tail == CORTEX_MSG_TAILER)
                                    {
                                        data_ptr tm_msg(new std::vector<uint8_t>(head, head + len));
                                        if (imp_->tm_msg_callback_fun_)
                                        {
                                            imp_->tm_msg_callback_fun_(tm_msg);
                                        }
                                        head = std::search(head + len, imp_->parse_buf_.end(), msg_head.begin(), msg_head.end());
                                    }
                                    else
                                    {
                                        // TODO:log frame error
                                        std::cerr << "[cortex_sti_parser]: "
                                                  << "telemetry message format error." << std::endl;
                                        //帧格式错误在当前帧头后继续寻找帧头
                                        head = std::search(head + sizeof(CORTEX_MSG_HEADER), imp_->parse_buf_.end(), msg_head.begin(), msg_head.end());
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }

                        if (head != imp_->parse_buf_.end())
                        {
                            imp_->parse_buf_.erase(imp_->parse_buf_.begin(), head);
                        }
                        else if (imp_->parse_buf_.size() > 3)
                        {
                            //预留3个字节防止包头跨包
                            imp_->parse_buf_.erase(imp_->parse_buf_.begin(), head - 3);
                        }
                    }
                }
                catch (std::exception e)
                {
                    std::cerr << "[cortex_sti_parser] catch: " << e.what() << std::endl;
                }
            });
        }
    }

    void cortex_sti_parser::stop()
    {
        if (imp_->is_running_)
        {
            imp_->is_running_ = false;
            if (imp_->parse_thread_.joinable())
            {
                imp_->data_buf_.quit();
                imp_->parse_thread_.join();

                imp_->data_buf_.reset();
                imp_->parse_buf_.clear();
            }
        }
    }

    void cortex_sti_parser::push_data(iterator_type begin, iterator_type end)
    {
        imp_->lost_count_ += (end - imp_->data_buf_.push(begin, end));
    }

    void cortex_sti_parser::set_tm_msg_callback_fun(const tm_msg_callback_fun_t &fun)
    {
        imp_->tm_msg_callback_fun_ = fun;
    }

    cortex_sti_parser::buffer_status cortex_sti_parser::get_buffer_status() const
    {
        return buffer_status{ imp_->data_buf_.capacity(), imp_->data_buf_.used_size(), imp_->lost_count_ };
    }
}  // namespace cortex
