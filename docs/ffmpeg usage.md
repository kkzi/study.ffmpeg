

### 抓取桌面推流

```shell
ffmpeg -f gdigrab -i desktop -s 800x400 -f mpegts udp://127.0.0.1:6666
```


### 播放
```shell
ffplay udp://127.0.0.1:6666
```

