#!/bin/bash
# $1 : url where files are located
# $2 : compressed file expected
# $3 - $N : list of files expected in case compressed files doesn't exist

pushd `dirname $0` &> /dev/null
MY_DIR=$(pwd)
popd &> /dev/null
retry="$MY_DIR/retry.py -s 1 -r 2"

usage(){
    echo "Usage: wget_unpack.sh URL EXPECTED_PACK FILE_LIST..."
    echo  "        URL:             url where files are located"
    echo  "        EXPECTED_PACK:   name of expected package"
    echo  "        FILE_LIST:       the arbitrary number of files that are expected in package" 
    echo  "                         and the file each should be copied to. ( : separated)"
    echo  "                         Ex: malloc.log:malloc.log.old"
}

# We will need the return codes from wget. If wget fails we have other logic to execute.
set +e

#stop the attempts if we get a 404, we will try something else in that case
# The '[ ]' in the regexp is necessary to avoid log parser false positives.
${retry} --stderr-regexp 'ERROR[ ]404: Not Found' --fail-if-match wget -O wget_unpack.tar.gz ${1}/${2}

getResult=$?

tar -xzvf wget_unpack.tar.gz

unpackResult=$?

set -e

# if these steps succeed, move the files to their target filenames
# if these steps fail, go get the files individually.

if [ $getResult -eq 0 ] && [ $unpackResult -eq 0 ]; then
    echo "Got the packed files"
    for ((i=3 ; i <= $# ; i++ )); do
        # grab sections before and after ':'
        front=${!i%:*}
        back=${!i#*:}

        if [ -z "$front" ] || [ -z "$back" ]; then
            usage
            exit 1
        fi

        echo "mv ./${front} ./${back}"
        mv ./${front} ./${back}
    done
else
    echo "Packed file not available, try getting individual files"
    for ((i=3 ; i <= $# ; i++ )); do
        # grab sections before and after ':'
        front=${!i%:*}
        back=${!i#*:}

        if [ -z "$front" ] || [ -z "$back" ]; then
            usage
            exit 1
        fi

        ${retry} --stderr-regexp 404 --fail-if-match wget -O ${back} ${1}/${front}
    done
fi
rm wget_unpack.tar.gz
