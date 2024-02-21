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

## Compiling

```
export LD_LIBRARY_PATH=/data/john/libs/lib
make clean && make
```

## Run

```
sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d -c 1073741824
```
