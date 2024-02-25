## libzbd

```
cd /data/john
mkdir -p libs

git clone https://github.com/westerndigitalcorporation/libzbd.git
cd libzbd
sh ./autogen.sh
./configure --prefix=/data/john/libs
make
make install
```

## libgcrypt

```
apt install libgcrypt20-dev
```

## uuid

```
apt install uuid-dev
```

## Compiling

```
export LD_LIBRARY_PATH=/data/john/libs/lib
make clean && make
```

## Run

```
sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d -c 1073741824
```

## GDB

```
sudo LD_LIBRARY_PATH=/data/john/libs/lib gdb --args ./znsccache -d /dev/nvme0n2 -c 1073741824
```

## Tidy

```
clang-format -i src/*.c include/*.h
```
