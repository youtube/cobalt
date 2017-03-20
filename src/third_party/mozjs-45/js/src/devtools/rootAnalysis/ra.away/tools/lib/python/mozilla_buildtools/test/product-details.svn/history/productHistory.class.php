<?php
    /**
     * Functions used by all product history classes. Instead of including this
     * class or calling its methods, include the class of the product of which
     * you need release history.
     *
     * @author Justin Scott <fligtar@mozilla.com>
     */
    class productHistory {

        /**
         * Returns an array with release version numbers as keys and their release
         * date as values.
         *
         * Example usage:
         *      include_once('product-details/history/firefoxHistory.class.php');
         *      $firefoxHistory = new firefoxHistory();
         *      $releases = $firefoxHistory->getReleaseDates(array('major'), 'version');
         *
         * @param $release_types array The type of releases to get. Can include
         *                              'major', 'stability', and 'development'.
         *                              Defaults to all available.
         * @param $sort_by string The sort order. Can be 'date' or 'version'.
         *                              Defaults to date.
         * @returns array
         */
        function getReleaseDates($release_types = array(), $sort_by = 'date') {
            // Set default release types if none passed
            if (empty($release_types))
                $release_types = array('major', 'stability', 'development');

            $releases = array();

            // Add major releases if requested
            if (in_array('major', $release_types))
                $releases = array_merge($releases, $this->major_releases);

            // Add stability releases if requested
            if (in_array('stability', $release_types))
                $releases = array_merge($releases, $this->stability_releases);

            // Add development releases if requested
            if (in_array('development', $release_types))
                $releases = array_merge($releases, $this->development_releases);


            // Sort array as requested
            if ($sort_by == 'date')
                asort($releases);
            elseif ($sort_by == 'version')
                ksort($releases);

            return $releases;
        }

    }

?>