TODO(cobalt): Remove the below section. Don't merge this into main without cleaning it up.

For the experimental/ozone branch:

How to run:
 * `git checkout origin/experimental/ozone`
   * If the above command can't find the branch, you may need to `git fetch --all` or sync gclient: `gclient sync --no-history --force -r $(git rev-parse @)`
 * `python cobalt/build/gn.py -p linux-x64x11 -c qa`
 * `autoninja -C out/linux-x64x11_qa cobalt`
 * `out/linux-x64x11_qa/cobalt`

Known Issues:
 * Cobalt window and content area sizes don't match. The window will be far larger than the displayed content. b/388348504
 * Passing focus to the content area is finicky. The easiest way I've found to do it is to click the back button on the navigation bar. b/388348048
 * Not all keyboard inputs work. Input mapping is still in progress. b/380286109
 * There is a navigation bar and the webapp isn't full screen within the content area. This is intentional for now to make other things easier, but can be removed with the flag `--content-shell-hide-toolbar` (though there will still be a border). b/388348307
