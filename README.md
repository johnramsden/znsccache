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
sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s 32768 -c ../credentials.json -f /home/john/ssd/zns/disk\=33554432KB\,obj\=32KB\,uuids\=1048576iter\=2097152.txt | tee /data/john/zns.log
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

## Benchmarks

```
timestamp,type,time (ms)
```

Record:
* hit rate vs time
* evictions vs time
* total get latency vs time
* Read disk latency vs time
* Write disk latency vs time

Bench:
* 32K obj   - chunk sz 16K, 32K   - evict, no evict - S3 geomean 32K
* 512K obj  - chunk sz 256K, 512K - evict, no evict - S3 geomean 512K
* 1M obj    - chunk sz 512K, 1M   - evict, no evict - S3 geomean 1M
* 256M obj  - chunk sz 128M 256M  - evict, no evict - S3 geomean 256M
* 1G obj    - chunk sz 512M 1G    - evict, no evict - S3 geomean 1G

* 32K obj   - chunk sz 16K, 32K   - evict, no evict - S3 geomean 32K

Using 57ms for bench

```
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=57 make
```

0=173.737650
1=129.817582
2=94.689707
3=77.347911
4=66.980935
5=66.326261
6=34.335777
7=41.448185
8=30.014788
9=35.582980
10=73.671910
11=39.032365
12=36.435687
13=48.025056
14=42.668519
stdev=39.15
geomean=57.36
avg=66.007688

* 512K obj  - chunk sz 256K, 512K - evict, no evict - S3 geomean 512K

Using 198ms for bench

0=299.297865
1=190.926778
2=228.409959
3=235.568989
4=210.793632
5=131.004768
6=112.267369
7=140.005505
8=166.498834
9=200.929175
10=221.518356
11=222.765190
12=306.344046
13=224.442063
14=185.046569
stdev=52.94
geomean=197.93
avg=205.054607

* 1M obj    - chunk sz 512K, 1M   - evict, no evict - S3 geomean 1M

Using 273ms for bench

0=411.901749
1=384.584974
2=319.476440
3=275.149861
4=286.418247
5=299.570968
6=187.382530
7=204.046372
8=232.422143
9=295.517201
10=300.988703
11=286.104114
12=258.754837
13=225.793748
14=217.126224
stdev=60.55
geomean=272.69
avg=279.015874

* 256M obj  - chunk sz 128M 256M  - evict, no evict - S3 geomean 256M

Using 5974ms for bench

0=19432.277805
1=9693.144624
2=8815.460454
3=6778.249581
4=6186.889682
5=5873.107870
6=5501.407963
7=4484.963589
8=4233.923278
9=3979.102497
10=4844.022035
11=6341.245626
12=4723.135366
13=4376.366384
14=4086.082339
stdev=3794.30
geomean=5974.46
avg=6623.291940

* 1G obj    - chunk sz 512M 1G    - evict, no evict - S3 geomean 1G

Using 24343ms for bench

0=43754.830457
1=27948.829392
2=20282.773869
3=24346.867982
4=18599.868245
5=19434.322517
6=21626.409352
7=20666.890445
8=20143.417247
9=18520.894672
10=16242.383070
11=35398.169103
12=32120.796345
13=30162.610940
14=30809.102791
stdev=7505.53
geomean=24343.18
avg=25337.211095
