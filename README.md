## libzbd

```
cd /data/john
mkdir libs

git clone https://github.com/westerndigitalcorporation/libzbd.git
cd libzbd
sh ./autogen.sh
./configure --prefix=/data/john/libs
make
make install
```

## Compiling

```
export LD_LIBRARY_PATH=/data/john/libs/lib
make
sudo LD_LIBRARY_PATH=/data/john/libs/lib ./zns
```
