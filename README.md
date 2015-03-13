# pili-camera-sdk-demo
An example based on the pili-camera-sdk.

## 使用
```
# get submodule
git submodule update --init

# update url
将 demo.c 中 RTMP_URL 改为你的推流地址

# build
./start_build.sh

# run
./build/demo ${YOUR_FLV_FILE_PATH}
```

## 更新 submodule

```
./update_submodule.sh
```