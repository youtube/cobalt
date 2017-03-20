#!/bin/bash

# $1 : name of the compressed file
# $2 : directory to move to
# $3 : username
# $4 : ssh key file
# $5 : target location
# rest : files to compress
set -e

pushd `dirname $0` &> /dev/null
MY_DIR=$(pwd)
popd &> /dev/null
retry="$MY_DIR/retry.py -s 1 -r 2"

usage()
{
    echo "Usage: pack_scp.sh COMP_FILE DIR USERNAME SSH_KEY TARGET FILES"
    echo  "        COMP_FILE: name of compressed file"
    echo  "        DIR:       directory to move to before compression"
    echo  "        USERNAME:  ssh username"
    echo  "        SSH_KEY:   ssh key file"
    echo  "        TARGET:    Target location"
    echo  "        FILES:     files to compress"
}

# if incorrect number of args
if [ $# -lt 6 ] ; then
    usage
    exit 1
fi

compfile=${1}
shift
dir=${1}
shift
username=${1}
shift
sshkey=${1}
shift
target=${1}
shift


#if directory does not exist
if [ ! -d $dir ] ; then
    echo "Error: Directory to compress from not found."
    usage
    exit 1
fi

# if ssh key does not exist
if [ ! -f "${HOME}/.ssh/$sshkey" ] ; then
    echo "Error: SSH key file does not exist"
    usage
    exit 1
fi

echo "tar -cz -C $dir -f $compfile  \"$@\""
tar -cz -C $dir -f $compfile "$@"

echo "${retry} scp -o User=$username -o IdentityFile=~/.ssh/$sshkey $compfile $target"
${retry} scp -o User=$username -o IdentityFile="~/.ssh/$sshkey" $compfile $target

rm $compfile
