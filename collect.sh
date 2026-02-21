#!/bin/sh -x

SAMPLES=3

test() {
kbps=$1
version=$2
mode=$3
quicstate=$4
quicvalue=$5
quicarg=$6

adb install ~/${version}gh/out/android-arm_qa/cobalt.apk

#for f in 16  32 128 512 1024 4096 16384 ; do
#for f in 00016  00128  01024  16384 ; do
for f in 01024  04096 16384 ; do
    echo ${f}
    logname=${version}lts_chunk_${f}k_${kbps}kbps_${mode}_quic_${quicstate}
    adb shell am force-stop dev.cobalt.coat
    adb shell input keyevent KEYCODE_HOME
    sleep 1
    adb shell am start --esa args "--url=http://localhost:8000/xhr.html?chunksize=${f}\&quic=${quicvalue}\&limitkbps=${kbps}\&mode=${mode},--allow_all_cross_origin${quicarg}" dev.cobalt.coat
    sleep 5 # start and stabilize
    adb shell 'top -n 5 -d 3 -b -p `pidof dev.cobalt.coat`' | egrep -v --line-buffered "Tasks: | Mem: |Swap: |sirq|^$" | tee ${logname}.log
    cat ${logname}.log | egrep dev.cobalt.coat | tr -s " " | cut -d " " -f 9 | awk '{sum+=$1; count+=1} END {print sum/count}' > ${logname}.summary
    cat ${logname}.summary
done

}

test 30000 24 xhr on 1
test 30000 24 xhr off 0 ,--disable_quic
#test 30000 24 fetch off 0 ,--disable_quic

test 30000 25 xhr on 1
test 30000 25 xhr off 0 ,--disable_quic
#test 30000 25 fetch off 0 ,--disable_quic
