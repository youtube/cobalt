#!/bin/bash
set -e
trap "rm -fv /tmp/signing.*" EXIT
while true; do
    jobs=""
    for i in `seq 5`; do
        (
        set -e
        t=$(mktemp /tmp/signing.XXX)
        n=$(mktemp /tmp/signing.XXX)
        f=$(mktemp /tmp/signing.XXX)
        dd if=/dev/urandom of=$f count=1000 > /dev/null 2>&1
        mar -c $f.mar $f
        curl --fail -k -Fslave_ip=127.0.0.1 -Fduration=60 --user foo:bar https://localhost:8081/token > $t 2>/dev/null
        #echo "waiting..."
        #sleep 30
        python ./signtool.py -c host.cert -H localhost:8081 -f mar -t $t -n $n -v $f.mar
        #./signmar -d .nss2 -n test -v $f.mar
        rm $t $n $f $f.mar
        )&
        jobs+=" $!"
    done
    for j in $jobs; do
        wait $j
    done
    #break
done
