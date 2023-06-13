
# study.ffmpeg 学习 ffmpeg 

> 学习 ffmpeg 优先读 examples ，其次 ffplay/ffplay 工具代码  
> 网上的资料大部分基于 4.0 以前的接口：调用流程虽然都一样，但是很容易陷入编译错误  

基于 ffmpeg 5.1 封装了几个常用流程

## ff_capture.h 抓取桌面

```cpp
// 抓桌面，存为 bmp 文件和 yuv 文件
ff_capture cap({ 0, 0, 400, 200 }, 10);
size_t count = 0;
cap.on_bmp_packet([&count](auto &&bmp) {
    std::ofstream file(std::format("bmp_{:04d}.bmp", count++), std::ios::binary | std::ios::trunc);
    file.write((char *)bmp->data, bmp->size);
});

std::ofstream yuv_file("desktop.yuv", std::ios::binary | std::ios::trunc);
cap.on_yuv_frame([&yuv_file](auto &&yuv) {
    ff_save_yuv_file(yuv_file, yuv);
});
cap.run();
```


## ff_encoder.h 编码和封装
```cpp
// 把 yuv 编码成 h264, 封装成 ts.
ff_encoder enc("mpegts", "", 400, 200, 10);
std::ofstream pktfile("output.264", std::ios::trunc | std::ios::binary);
enc.on_enc_packet([&pktfile](auto &&packet) {
    pktfile.write((char *)packet->data, packet->size);
    pktfile.flush();
});

std::ofstream ts_file("output.ts", std::ios::trunc | std::ios::binary);
enc.on_mux_packet([&ts_file](auto &&buf, auto &&len) {
    ts_file.write((char *)buf, len);
    ts_file.flush();
});

// 可以读 yuv 文件，或结合 ff_capture.on_yuv_frame 
// enc.encode(yuv_frame);


// 或者封装完成后推 RTP
ff_encoder rtpenc("mpegts", "rtp:://233.3.3.3:1234", 400, 200, 10);

```

## ff_decoder.h 解封装和解码
```cpp
// 收 RTP 数据，解码成 yuv frame
{
    ff_decoder dec("rtp://234.1.1.1:1234");
    dec.on_frame([](auto &&frame) {
        printf("[%x] pts %lld\n", std::this_thread::get_id(), frame->pts);
    });
    dec.run();
}

// 从文件/内存读取数据，解码成 yuv frame
{
    ff_decoder dec;
    dec.on_frame([](auto &&frame) {
        printf("[%x] pts %lld\n", std::this_thread::get_id(), frame->pts);
    });

    std::thread enc_trd([&dec] {
        dec.run();
    });

    std::ifstream ts_file("input.ts", std::ios::binary);
    while (ts_file.good())
    {
        static constexpr auto len = 188;
        uint8_t frame[len]{ 0 };
        ts_file.read((char *)&frame[0], len);
        dec.push_bytes(frame, len);
    }
    enc_trd.join();
}

```


