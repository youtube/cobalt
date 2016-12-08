These test whether the presence of the CSS MTM filter affect the path taken in
rendering the video replaced box. <normal.html> has a video tag without the
filter, while <mtm.html> has a video with the filter applied to it.

To test locally, all 3 of mtm.html, normal.html and progressive.mp4 have to be
hosted on a server that supports HTTP RANGE header, which is needed for video to
be loaded correctly. One such server is the extension to the Python
SimpleHTTPServer at https://github.com/danvk/RangeHTTPServer (run from this
directory, "cobalt/src/cobalt/browser/testdata/mtm-demo/"):

  $ sudo apt-get install python-pip
  $ pip install --user rangehttpserver
  $ python -m RangeHTTPServer

Test video without the MTM filter (should render normally, run from cobalt/src
directory):

  $ out/linux-x64x11_debug/cobalt --csp_mode=disable --allow_http --url=http://localhost:8000/normal.html

Test the video with the MTM filter (should NOT render in its bounding box in the
document, and in the presence of the correct rasterizer, should be rendered onto
its own texture off the main UI layout):

  $ out/linux-x64x11_debug/cobalt --csp_mode=disable --allow_http --url=http://localhost:8000/mtm.html

There is also the option of using the google cloud utils and the cloud storage
to publish these files, as is done with the other demos. Care should be taken
that nothing confidential gets uploaded with these files. "public-read" is
needed in the ACL because linking and video srcing does not work without it.

A bucket specifically for this demo already exists at "gs://yt-cobalt-mtm-test",
but uploading to it often might be problematic since buckets are not verison-
controlled:

  $ gsutil cp -a public-read cobalt/browser/testdata/mtm-demo/normal.html cobalt/browser/testdata/mtm-demo/mtm.html cobalt/browser/testdata/mtm-demo/progressive.mp4 gs://yt-cobalt-mtm-test/
  $ out/linux-x64x11_debug/cobalt --csp_mode=disable --url=https://storage.googleapis.com/yt-cobalt-mtm-test/mtm.html
  $ out/linux-x64x11_debug/cobalt --csp_mode=disable --url=https://storage.googleapis.com/yt-cobalt-mtm-test/normal.html
