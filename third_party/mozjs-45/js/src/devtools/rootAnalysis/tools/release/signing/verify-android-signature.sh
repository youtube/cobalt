#!/bin/bash

function usage {
    echo "usage: $0 --apk=file.apk --tools-dir=dir [-n|-r]"
    echo -e "Required Parameters:"
    echo -e "\t   --apk=path: the apk file to verify"
    echo -e "\t     Will be downloaded if argument is a url (preceded by http:// for ex)."
    echo -e "\t   --tools-dir=dir: path to the tools directory\n"
    echo -e "\t   --nightly | -n: nightly package"
    echo -e "\tor --release | -r: release package"
    echo -e "Optional Parameters:"
    echo -e "\t   --help | -h: display this usage message"
}

#Parse arguments
#Note that last of [-n|-r] flags will be used
for i in $@; do
    case $i in
    --apk=*)
        APKPATH=${i#*=}
        if [[ $APKPATH =~ "://" ]]; then
            curl -s $APKPATH > package.apk 
            APKPATH="./package.apk"
        fi
        if [ ! -f $APKPATH ]; then
            echo "ERROR: $APKPATH not a valid file." >&2
            exit 1
        fi
        ;;
    --tools-dir=*)
        TOOLSDIR=${i#*=}
        if [ ! -d $TOOLSDIR ]; then
            echo "ERROR: Tools directory not a valid directory."
            exit 1
        fi
        ;;
    --nightly|-n)
        SIGFILE="NIGHTLY.DSA"
        COMPFILE="nightly"
        ;;
    --release|-r)
        SIGFILE="RELEASE.RSA"
        COMPFILE="release"
        ;;
    --help|-h)
        usage
        exit 0
        ;;
    *)
        echo "ERROR: Invalid argument $i"
        exit 1
        ;;
    esac
done
#Verify required command line arguments passed
echo "TOOLS DIR: $TOOLSDIR"
if [ ! $APKPATH ]; then
    echo "--apk is required" >&2
    usage
    exit 1
elif [ ! $TOOLSDIR ]; then
    echo "--tools-dir dir is required" >&2
    usage
    exit 1
elif [ ! $SIGFILE ]; then
    echo "-n or -r required" >&2
    usage
    exit 1
elif [ ! -f "$TOOLSDIR/release/signing/$COMPFILE.sig" ]; then
    echo "ERROR: Could not find $TOOLSDIR/release/signing/$COMPFILE.sig"
    exit 1
fi

unzip $APKPATH META-INF/$SIGFILE  #unzip the signing info from the apk
if [ $? -ne 0 ]; then
    echo "ERROR: Could not unzip $APKPATH" >&2
    echo "Are you sure this is a $COMPFILE package?" >&2
    rm -rf ./META-INF
    exit 1
fi
if [ ! -f "META-INF/$SIGFILE" ]; then
    echo "ERROR: No signature file present" >&2
    rm -rf ./META-INF
    exit 1
fi

#Get the signature information from the apk package
SIGOUT=`keytool -printcert -file META-INF/$SIGFILE`
#The piping used in the diff *may* be a bash-only feature.
DIFF=`diff -I "Valid from:" -I "SHA256:" $TOOLSDIR/release/signing/$COMPFILE.sig <(echo "$SIGOUT")`
RET=$?
rm -rf ./META-INF    #no longer need the unpackaged apk
if [ $RET -eq 0 ]; then
    echo "Valid"
else
    echo "Invalid"
fi

exit $RET

