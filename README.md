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

## GDB

```
sudo valgrind env LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s 131072 -c ../credentials.json -f /home/john/ssd/zns/disk=268435456KB,obj=32KB,uuids=16777216iter=33554432.txt
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
Uploading objects of size 268435456 random bytes to znscache.small
6c228e61-abb7-4100-a5df-417f25b47c36, 's'
cf752bc3-b33f-435f-ac30-178e971f54a2, 't'
7c968f5a-0c97-4cd8-960b-1c1bf6ab67ee, 'u'
c3f16f76-8f94-40f1-950d-695bb226fe43, 'v'
54b4afb0-7624-448d-9c52-ddcc56e56bf2, 'w'
3f5d58a4-1780-46b5-bd96-d4c5b4f55b2b, 'x'
e5a19178-6b77-40b7-9bf8-f45f70ea19a5, 'y'
13f9c273-3f2f-486a-8003-ba2fba58b258, 'z'
c75b2775-f71e-47f9-9500-530c8cf9f344, 'a'
e5be69c3-6e22-4c2c-aa09-0bff9417c361, 'b'
982aff04-b7b6-4060-a141-241af8c46375, 'c'
62c0ae68-e637-4ddc-a0aa-aa1151f88595, 'd'
7fc3ca1b-3968-4274-9a5b-688f01dca81b, 'e'
b9cece7d-50ce-4f00-9abb-4e9c07ed68fd, 'f'
7446cbb0-430b-44c4-9da9-443a044c14d1, 'g'
8c0630ac-c4b5-4bf1-a7a3-13a7e95032dd, 'h'
Finished uploading. Total uploaded size: 4294967296 bytes.
```

## TODO

* Eviction (foreground)
* Add SSD backend

## Eviction

x chunks in y zones

If using LRU I get "holes" if chunk < zone on evict
Remove 1, rewrite zone?

If zone == chunk

Eviction is delete 1, write 1

---

Keep a few free zones? Background defrag/evict by LRU?

If < n zones have no free chunks remaining, LRU x chunks, migrate to new zone(s), clear old zones

--

GC algos

Key params:

Y axis: performance

X axis:

```
10 zones remain, evicting.
Evicting zone 20.
Evicting zone 4294967295.
```
