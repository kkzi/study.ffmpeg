#include "cortex_tm_parser.h"
#include "cortex_time.h"
#include "lockfree_spsc_queue.h"

namespace cortex
{
    struct cortex_tm_parser::cortex_tm_parser_imp_t
    {
        cortex_tm_parser_imp_t(size_t capacity)
            : data_buf_(capacity)
        {
        }

        std::atomic_bool is_running_ = false;
        std::atomic_uint64_t lost_count_ = 0;
        lockfree_spsc_queue<data_ptr> data_buf_;
        std::thread process_thread_;
    };

    cortex_tm_parser::cortex_tm_parser(size_t capacity, int time_code)
        : imp_(new cortex_tm_parser_imp_t(capacity))
        , time_code_(time_code)
    {
    }

    cortex_tm_parser::~cortex_tm_parser()
    {
    }

    bool cortex_tm_parser::is_running() const
    {
        return imp_->is_running_;
    }

    void cortex_tm_parser::start()
    {
        if (!imp_->is_running_)
        {
            imp_->is_running_ = true;
            imp_->lost_count_ = 0;
            //清空计数
            imp_->process_thread_ = std::thread([=]() {
                while (imp_->is_running_)
                {
                    auto tm_msgs = imp_->data_buf_.pop();
                    for (auto ptm_msg : *tm_msgs)
                    {
                        parse_tm_msg(ptm_msg);
                    }
                }
            });
        }
    }

    void cortex_tm_parser::stop()
    {
        if (imp_->is_running_)
        {
            imp_->is_running_ = false;
            if (imp_->process_thread_.joinable())
            {
                imp_->data_buf_.quit();
                imp_->process_thread_.join();
                imp_->data_buf_.reset();
            }
        }
    }

    void cortex_tm_parser::push_tm_msg(const data_ptr ptm_msg)
    {
        if (!imp_->data_buf_.push(ptm_msg))
        {
            imp_->lost_count_++;
        }
    }

    void cortex_tm_parser::set_tm_frame_callback_fun(const tm_frame_callback_fun_t &fun)
    {
        tm_frame_callback_fun_ = fun;
    }

    cortex_tm_parser::buffer_status cortex_tm_parser::get_buffer_status() const
    {
        return buffer_status{ imp_->data_buf_.capacity(), imp_->data_buf_.used_size(), imp_->lost_count_ };
    }

    /***************************************************************
     * @class hdr_tm_parser
     * @brief HDR.
     ***************************************************************/
#define HDR_FRAME_LENGTH_OFFSET 9     // 帧长所在偏移
#define HDR_TM_BLOCK_SIZE_OFFSET 12   // 遥测数据块大小所在偏移
#define HDR_TM_BLOCK_NUM_OFFSET 13    // 当前遥测消息内包含的数据块个数所在偏移
#define HDR_FIRST_TM_BLOCK_OFFSET 19  // 第一个遥测数据块偏移

    hdr_tm_parser::hdr_tm_parser(size_t capacity, int time_code)
        : cortex_tm_parser(capacity, time_code)
    {
    }

    hdr_tm_parser::~hdr_tm_parser()
    {
    }

    void hdr_tm_parser::parse_tm_msg(const data_ptr ptm_msg)
    {
        if (ptm_msg->size() > (HDR_FIRST_TM_BLOCK_OFFSET + 1) * sizeof(int32_t))
        {
            std::vector<uint8_t> vec_4bytes(sizeof(int32_t), 0x0);
            uint32_t first_tm_block_pos = HDR_FIRST_TM_BLOCK_OFFSET * sizeof(int32_t);

            uint32_t frame_len, tm_block_size, tm_block_num;
            memcpy(vec_4bytes.data(), ptm_msg->data() + HDR_FRAME_LENGTH_OFFSET * sizeof(int32_t), sizeof(int32_t));
            std::reverse(vec_4bytes.begin(), vec_4bytes.end());
            memcpy(&frame_len, vec_4bytes.data(), sizeof(int32_t));

            memcpy(vec_4bytes.data(), ptm_msg->data() + HDR_TM_BLOCK_SIZE_OFFSET * sizeof(int32_t), sizeof(int32_t));
            std::reverse(vec_4bytes.begin(), vec_4bytes.end());
            memcpy(&tm_block_size, vec_4bytes.data(), sizeof(int32_t));
            tm_block_size *= 8;  //(in 64-bit words)

            memcpy(vec_4bytes.data(), ptm_msg->data() + HDR_TM_BLOCK_NUM_OFFSET * sizeof(int32_t), sizeof(int32_t));
            std::reverse(vec_4bytes.begin(), vec_4bytes.end());
            memcpy(&tm_block_num, vec_4bytes.data(), sizeof(int32_t));

            if (tm_block_num < 131072)
            {
                //循环提取每一个数据块内的遥测帧
                for (uint32_t i = 0; i < tm_block_num; i++)
                {
                    if (!cortex_tm_parser::is_running())
                    {
                        break;
                    }
                    tm_frame_ptr pframe = std::make_shared<tm_frame>();
                    pframe->ptm_msg = ptm_msg;
                    pframe->frame_offset += (first_tm_block_pos + i * tm_block_size);
                    pframe->frame_len = frame_len;
                    auto index = std::ceil((double)pframe->frame_len / 8) * 8;
                    auto time = parse_crtx_time(time_code_, pframe->ptm_msg->data(), pframe->frame_offset + index);
                    tm_frame_callback_fun_(time, pframe);
                }
            }
        }
    }

    crt_tm_parser::crt_tm_parser(size_t capacity, int time_code)
        : cortex_tm_parser(capacity, time_code)
    {
    }

    void crt_tm_parser::parse_tm_msg(const data_ptr ptm_msg)
    {
        if (ptm_msg->size() > 17 * sizeof(int32_t))
        {
            std::vector<uint8_t> vec_4bytes(sizeof(int32_t), 0x0);
            uint32_t frame_len;
            memcpy(vec_4bytes.data(), ptm_msg->data() + 10 * sizeof(int32_t), sizeof(int32_t));
            std::reverse(vec_4bytes.begin(), vec_4bytes.end());
            memcpy(&frame_len, vec_4bytes.data(), sizeof(int32_t));

            tm_frame_ptr pframe = std::make_shared<tm_frame>();
            pframe->ptm_msg = ptm_msg;
            pframe->frame_offset += 16 * sizeof(int32_t);
            pframe->frame_len = frame_len;

            auto time = parse_crtx_time(time_code_, ptm_msg->data(), 3 * sizeof(int32_t));
            tm_frame_callback_fun_(time, pframe);
        }
    }

}  // namespace cortex
