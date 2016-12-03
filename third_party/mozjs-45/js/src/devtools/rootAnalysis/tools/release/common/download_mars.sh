pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
retry="$MY_DIR/../../buildfarm/utils/retry.py -s 1 -r 3"

download_mars () {
    update_url="$1"
    only="$2"
    test_only="$3"

    max_tries=5
    try=1
    # retrying until we get offered an update
    while [ "$try" -le "$max_tries" ]; do
        echo "Using  $update_url"
        # retrying until AUS gives us any response at all
        cached_download update.xml "${update_url}"

        echo "Got this response:"
        cat update.xml
        # If the first line after <updates> is </updates> then we have an
        # empty snippet. Otherwise we're done
        if [ "$(grep -A1 '<updates>' update.xml | tail -1)" != "</updates>" ]; then
            break;
        fi
        echo "Empty response, sleeping"
        sleep 5
        try=$(($try+1))
    done

    echo; echo;  # padding

    mkdir -p update/
    if [ -z $only ]; then
      only="partial complete"
    fi
    for patch_type in $only
      do
      line=`fgrep "patch type=\"$patch_type" update.xml`
      grep_rv=$?

      if [ 0 -ne $grep_rv ]; then
	      echo "FAIL: no $patch_type update found for $update_url" 
	      return 1
      fi

      command=`echo $line | sed -e 's/^.*<patch //' -e 's:/>.*$::' -e 's:\&amp;:\&:g'`
      eval "export $command"

      if [ "$test_only" == "1" ]
      then
        echo "Testing $URL"
        curl -k -s -I -L $URL
        return
      else
        cached_download "update/${patch_type}.mar" "${URL}"
      fi
      if [ "$?" != 0 ]; then
        echo "Could not download $patch_type!"
        echo "from: $URL"
      fi
      actual_size=`perl -e "printf \"%d\n\", (stat(\"update/$patch_type.mar\"))[7]"`
      actual_hash=`openssl dgst -$hashFunction update/$patch_type.mar | sed -e 's/^.*= //'`
      
      if [ $actual_size != $size ]; then
	      echo "FAIL: $patch_type from $update_url wrong size"
	      echo "FAIL: update.xml size: $size"
	      echo "FAIL: actual size: $actual_size"
	      return 1
      fi
      
      if [ $actual_hash != $hashValue ]; then
	      echo "FAIL: $patch_type from $update_url wrong hash"
	      echo "FAIL: update.xml hash: $hashValue"
	      echo "FAIL: actual hash: $actual_hash"
	      return 1
      fi

      cp update/$patch_type.mar update/update.mar
      echo $actual_size > update/$patch_type.size

    done
}
