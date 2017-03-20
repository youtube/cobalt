<?php

/**
 * Manages a list of region/country names which are localized
 *
 * @author James Long <jlong@mozilla.com>
 */
class regionDetails {

    function readProperties($file) {
        if (!is_file($file)) {
            return array();
        }

        $lines = file($file, FILE_SKIP_EMPTY_LINES);
        $ret = array();

        // we parse the $source array and remove white space
        // we only add to the array lines with a = delimiter
        foreach($lines as $line) {
            $vals = explode("=", $line);
            if(count($vals)>1) {
                $ret[trim($vals[0])] = trim($vals[1]);
            }
        }

        return $ret;
    }

    function getRegionNames($lang='en-US') {
        $root = dirname(__FILE__);
        $file = "$root/regions/$lang.properties";
        $props = $this->readProperties($file);

        asort($props, SORT_LOCALE_STRING);
        return $props;
    }

    function test() {
        $regions = $this->getRegionNames('en-US');

        if($regions['bm'] != 'Bermuda') {
            throw new Exception('region failed: Bermuda');
        }

        if($regions['aw'] != 'Aruba') {
            throw new Exception('region failed: Bermuda');
        }

        $regions = $this->getRegionNames('fr');

        if($regions['bj'] != 'Bénin') {
            throw new Exception('region failed: Bénin');
        }

        echo "Tests succeeded";
    }
}

?>
