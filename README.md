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

Set:

* xf
* zones
* file
* count
* -s bytes
* -m output

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
<!-- * 512K obj  - chunk sz 256K, 512K - evict, no evict - S3 geomean 512K -->
* 1M obj    - chunk sz 512K, 1M   - evict, no evict - S3 geomean 1M
<!-- * 256M obj  - chunk sz 128M 256M  - evict, no evict - S3 geomean 256M -->
* 1G obj    - chunk sz 512M 1G    - evict, no evict - S3 geomean 1G

* 32K obj   - chunk sz 16K, 32K   - evict, no evict - S3 geomean 32K

Using 57ms for bench

```c
#define EVICT_THRESH_ZONES_LEFT 1
#define EVICT_THRESH_ZONES_REMOVE 2
```

Evict

'disk=10485760KB,obj=32KB,uuids=26214400iter=2621440.txt'

No Evict:

'disk=41943040KB,obj=1024KB,uuids=40960iter=327680.txt'

```
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=57 make
```

144.738501,
98.838001,
107.908126,
98.918494,
118.180804,
116.020368,
60.175368,
37.280651,
58.458616,
35.930335,
42.303642,
31.601407,
36.354112,
30.871992,
100.580836,
53.483931,
36.369907,
34.819342,
32.351520,
31.446523,
36.295667,
41.905635,
33.363852,
29.932280,
36.685492,
34.027327,
33.672402,
33.987916,
38.897166,
34.277723,
34.772344,
36.018225,
35.033560,
32.214941,
36.892214,
33.702793,
34.758242,
35.443970,
67.553659,
36.191228,
36.559560,
38.053496,
34.650326,
31.218108,
31.639598,
38.053472,
39.905578,
37.811444,
33.288950,
32.699743,
34.919856,
34.030839,
33.912846,
32.659927,
34.068531,
37.473022,
32.584065,
34.812547,
32.297873,
30.739199,
37.814750,
58.374286,
44.341046,
30.071477,
37.168953,
38.416765,
41.087754,
33.353333,
29.139559,
40.512590,
34.713584,
36.020293,
33.374929,
32.113547,
38.850767,
37.482747,
30.991440,
38.285367,
29.306384,
59.654151,
36.210850,
35.197976,
32.719741,
35.170658,
31.322904,
35.549491,
30.584413,
34.622259,
37.709964,
34.463742,
34.518399,
34.468402,
36.018905,
33.288947,
36.133134,
34.596191,
33.574634,
37.633029,
40.193240,
31.660826,

geomean=39.08033715816217
stdev=20.75347930627147
avg=66.007688

<!-- * 512K obj  - chunk sz 256K, 512K - evict, no evict - S3 geomean 512K

Using 198ms for bench

```
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=198 make
```

Evict

disk=16GB
obj=512KB
uuids=2621440
iter=262144

No Evict:

disk=16GB
obj=512KB
uuids=32768
iter=262144


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
avg=205.054607 -->

* 1M obj    - chunk sz 512K, 1M   - evict, no evict - S3 geomean 1M

Evict:

'disk=41943040KB,obj=1024KB,uuids=3276800iter=327680.txt'

No Evict:

'disk=41943040KB,obj=1024KB,uuids=40960iter=327680.txt'

Using 123ms for bench

```c
#define EVICT_THRESH_ZONES_LEFT 1
#define EVICT_THRESH_ZONES_REMOVE 8
#define ZONES_USED 40
```

```shell
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=123 make
s=1048576
for f in 'disk=41943040KB,obj=1024KB,uuids=3276800iter=327680' 'disk=41943040KB,obj=1024KB,uuids=40960iter=327680'; do time sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s $s -c ../credentials.json -f ../"$f".txt -m /data/john/metric/"${f}".csv; done
```

379.407507,
324.466045,
304.762546,
261.600677,
239.067244,
197.643438,
193.684361,
180.012219,
162.592104,
165.724330,
267.777171,
163.222187,
168.060735,
163.174782,
165.704061,
204.354201,
162.527558,
148.731770,
149.756350,
144.010244,
151.974617,
132.640099,
136.106890,
148.159048,
164.235069,
164.391175,
146.178410,
151.505337,
151.179584,
152.841972,
242.451406,
129.444552,
133.178374,
131.760989,
145.595123,
152.420751,
115.732278,
116.811031,
116.184848,
114.289081,
123.797955,
121.154719,
131.107601,
118.613192,
115.533624,
105.778838,
102.630257,
111.097905,
103.145199,
96.450548,
111.991947,
101.450951,
106.658781,
101.503723,
102.448760,
102.562838,
97.514699,
100.894683,
101.198875,
102.536089,
108.910146,
89.400979,
87.905842,
86.216650,
93.507178,
87.545124,
91.324082,
85.415193,
84.636622,
84.046763,
87.196268,
84.326796,
86.099441,
86.810106,
86.173009,
88.940620,
92.195960,
86.064256,
81.124127,
100.756241,
176.491150,
99.678376,
100.785342,
97.823395,
103.273104,
103.159409,
106.751627,
100.171662,
100.848039,
100.186507,
130.413496,
99.540712,
99.922896,
95.540788,
97.713666,
198.523299,
85.312468,
88.189138,
88.695380,
83.824218,
geomean=123.24246908284057
stdev=54.36151694114781
<!-- * 256M obj  - chunk sz 128M 256M  - evict, no evict - S3 geomean 256M

Using 5974ms for bench

disk=16777216KB,obj=262144KB,uuids=64iter=512
disk=16777216KB,obj=262144KB,uuids=5120iter=512

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
avg=6623.291940 -->

* 1G obj    - chunk sz 512M 1G    - evict, no evict - S3 geomean 1G

Using 24343ms for bench

```c
#define EVICT_THRESH_ZONES_LEFT 1
#define EVICT_THRESH_ZONES_REMOVE 20
#define ZONES_USED 100
```

```shell
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=16705 make
s=1073741824
for f in 'disk=104857600KB,obj=1048576KB,uuids=8000iter=800' 'disk=104857600KB,obj=1048576KB,uuids=100iter=800'; do time sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s $s -c ../credentials.json -f ../"$f".txt -m /data/john/metric/"${f}".csv; done
```

Evict:

'disk=104857600KB,obj=1048576KB,uuids=8000iter=800.txt'

No Evict:

'disk=104857600KB,obj=1048576KB,uuids=100iter=800.txt'


31742.873934,
20459.921846,
16902.393337,
13546.954396,
15273.340304,
11994.791415,
11758.152278,
10777.139775,
10871.532194,
11983.420855,
10886.705776,
11063.549937,
10945.401134,
13324.133361,
11112.815445,
11251.658074,
13924.043155,
11014.023071,
37683.249850,
24814.537191,
20653.686123,
19499.157451,
18427.312826,
15931.091545,
14961.991679,
13972.097067,
12936.662582,
11780.197856,
10959.263361,
12327.458443,
12680.273007,
10940.531734,
11299.680058,
37958.165823,
33148.809639,
26662.552847,
23368.945497,
19471.527772,
18066.624907,
17507.943025,
13863.598802,
13308.492780,
11170.789201,
10898.509202,
11076.848490,
15592.741362,
37776.855549,
24481.304154,
17931.751409,
18058.231281,
12805.444865,
30234.241833,
39333.221743,
36547.493431,
20231.357210,
38013.027083,
40259.754038,
21216.849075,
15809.681471,
15897.299951,
18149.367138,
14628.377541,
12057.099581,
12114.405607,
14343.916081,
11683.515276,
11607.731590,
11627.871372,
10970.413344,
10857.167010,
13122.530736,
11257.146840,
41844.450488,
32068.518909,
30608.224092,
22599.389567,
22018.496450,
25119.057508,
20138.893412,
21157.045457,
19701.652088,
13252.647489,
39225.065400,
24026.787373,
23220.207190,
17358.841635,
15225.278076,
13179.112845,
13834.529151,
10915.075062,
11492.634303,
10939.197258,
10934.731912,
12756.264925,
10784.303746,
11050.952335,
11087.191851,
14068.162675,
40537.651630,
24682.300882,
stdev=8768.10166286344
geomean=16705.160649196725

```
for s in hitrate evictions get read write freezones cached uncach finishzone; do echo "timestamp,type,timems" > $s.csv; grep $s obj* >> $s.csv; done
```

---

            Runtime,        gets
32K,noevict: 11002092.938818, 3000000
32K,evict: 14401014.269159, 912859
1M,noevict: 7515494.630467, 3000000
1M,evict: 14401063.336236, 256022
