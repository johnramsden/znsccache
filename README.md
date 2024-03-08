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
sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s 4096 -c ../credentials.json
```

## GDB

```
sudo LD_LIBRARY_PATH=/data/john/libs/lib gdb --args ./znsccache -d /dev/nvme0n2 -s 4096 -c ../credentials.json
```

## Tidy

```
clang-format -i src/*.c include/*.h
```

## AWS libs3

```
apt install libs3-dev
```

## cJSON

```
apt install libcjson-dev
```

## Data

### znscache.small

```
Uploaded 536870912 bytes to znscache.small with key e8ade124-f00a-47ea-aa7d-0dbd55dd0866
Uploaded 536870912 bytes to znscache.small with key cff380c9-c5ab-441e-9932-dab22eb1b0ec
Uploaded 536870912 bytes to znscache.small with key ba429e73-d963-41c6-9681-863f9406d2c4
Uploaded 536870912 bytes to znscache.small with key 81cf4f93-654c-487a-90a5-e0a545a9d715
Uploaded 536870912 bytes to znscache.small with key 1d4970a9-9138-4f2f-b42f-d0e06badd01e
Uploaded 536870912 bytes to znscache.small with key 42f1c47b-d4ed-432f-96c6-6450cbd30ce9
Uploaded 536870912 bytes to znscache.small with key 42ca739b-c16d-4b61-be50-949450399ce8
Uploaded 536870912 bytes to znscache.small with key 327d518e-dc56-4919-ab61-5b814f93b441
Finished uploading. Total uploaded size: 4294967296 bytes.
```
