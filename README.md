# pili-camera-sdk-demo
An example based on the pili-camera-sdk.

## 编译

```
$ mkdir build
$ cd build
$ cmake -DOS_ARCH=1 ..
$ make
```

其中 OS_ARCH 根据编译平台不同而选择

- darwin_amd64
- linux_amd64
- linux_386

## 运行

```
demo ${FLV_FILE_PATH} %{YOUR_PUSH_URL}
```