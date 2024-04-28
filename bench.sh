#!/bin/sh

dir=/home/john/ssd/zns/benchmarks/

# 'obj=1Mevict=nodb=7uuids=7168iter=3000000.txt'
# 'obj=1Mevict=yesdb=100uuids=102400iter=3000000.txt'
# 'obj=32Kevict=nodb=7uuids=229376iter=3000000.txt'
# 'obj=32Kevict=yesdb=100uuids=3276800iter=3000000.txt'
export LD_LIBRARY_PATH=/data/john/libs/lib

# make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=39 make

# s=32768
# for f in 'obj=32Kevict=yesdb=400uuids=13107200iter=3000000' 'obj=32Kevict=nodb=7uuids=229376iter=3000000'; do
#     echo "$f"
#     sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s $s \
#     -c ../credentials.json -f $dir/"$f".txt -m /data/john/metric/"${f}".csv;
# done

# s=1048576
# make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=123 make
# for f in 'obj=1Mevict=yesdb=400uuids=409600iter=3000000' 'obj=1Mevict=nodb=7uuids=7168iter=3000000'; do
#     echo "$f"
#     sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s $s \
#     -c ../credentials.json -f $dir/"$f".txt -m /data/john/metric/"${f}".csv;
# done
s=1048576
make clean && DEBUG=0 EMULATE_S3=1 S3_DELAY=123 make
for f in 'obj=1Mevict=nodb=7uuids=7168iter=15000000'; do
    echo "$f"
    sudo LD_LIBRARY_PATH=/data/john/libs/lib ./znsccache -d /dev/nvme0n2 -s $s \
    -c ../credentials.json -f $dir/"$f".txt -m /data/john/metric/"${f}".csv;
done
