This folder contains homebrew packages required by Cobalt MacOS builds.  Stock
Kokoro images do not include the packages we need, so we have to install these
at build time. We've filed request (b/307764855) to have these added to the
stock MacOS image.

Due to network restrictions in the internal MACOS clusters we can't download these
packages from the internet.

Follow these direction to add new package:

1. Configure your MacOS jobs to run on MACOS_EXTERNAL cluster. This will allow to
download and install packages from the web.
2. Add the following to internal/kokoro/bin/build_mac.sh and make CL:

+HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 HOMEBREW_NO_INSTALL_FROM_API=1 \
  brew install sccache
+find /Users/kbuilder/Library/Caches/Homebrew/downloads/ -name \*sccache\* -print
+find /Users/kbuilder/Library/Caches/Homebrew/downloads/ -name \*sccache\*.json -print | xargs cksum
+find /Users/kbuilder/Library/Caches/Homebrew/downloads/ -name \*sccache\*.json -exec cat {} +
+exit 0

Running aforementioned commands on an external cluster will install homebrew package and
download required json and tarball to /Users/kbuilder/Library/Caches/Homebrew/downloads/.

3. Inspect the log to find the url from which tar.gz was downloaded and find a filename for
a given package.  These will be located in /Users/kbuilder/Library/Caches/Homebrew/downloads/.
The log will contain something like so:

+ brew install sccache
==> Fetching sccache
==> Downloading https://ghcr.io/v2/homebrew/core/sccache/manifests/0.3.3
==> Downloading https://ghcr.io/v2/homebrew/core/sccache/blobs/sha256:4a1cd85ee1e348804ab9fe9de9179beb22c56ce63c4e31b233fd5b8e63b79b61
==> Downloading from https://pkg-containers.githubusercontent.com/ghcr1/blobs/sha256:4a1cd85ee1e348804ab9fe9de9179beb22c56ce63c4e31b233fd5b8e63b79b61?se=2023-10-19T23%3A35%3A00Z&sig=%2FbqdX5aTdpPhaErKNvV%2FxRb1Pt9DEBXWPklvGY%2FYzQI%3D&sp=r&spr=https&sr=b&sv=2019-12-12
==> Pouring sccache--0.3.3.big_sur.bottle.tar.gz
🍺  /usr/local/Cellar/sccache/0.3.3: 7 files, 19.8MB
+ find /Users/kbuilder/Library/Caches/Homebrew/downloads/ -name \*sccache\* -print
a6ec3b928b282acc7b6cf46d235af9c833e84ca4308344172a706d12a9e1a3c6--sccache--0.3.3.big_sur.bottle.tar.gz
0be08a7c5649358151d6f8c7d991a8e1bf2b5d1205d946490fa3068abe7fa892--sccache-0.3.3.bottle_manifest.json

Locate sccache tar.gz file name and the url from which it was downloaded.  Then, download tar.gz using the
following command:

curl -L -H "Authorization: Bearer QQ==" \
  -o a6ec3b928b282acc7b6cf46d235af9c833e84ca4308344172a706d12a9e1a3c6--sccache--0.3.3.big_sur.bottle.tar.gz \
  https://ghcr.io/v2/homebrew/core/sccache/blobs/sha256:4a1cd85ee1e348804ab9fe9de9179beb22c56ce63c4e31b233fd5b8e63b79b61

4. Find the content of package manifest json file in the log, copy it, and create a file with the same
name and content.

5. Run cksum and compare your new file with what you see in the log to make sure json file you created is identical to
the one downloaded by brew.  Sometimes newline is not displayed properly in the log, or you might have to remove
newline character via `truncate -s -1 filename`).  Checksum must match for both files, otherwise homebrew will make an attempt
to download from the web.

6. Commit both files and remove 'find' and exit 0' commands from build_mac.sh.
7. Inspect new build log to make sure brew archives no longer being downloaded from the web.  If everything
worked properly you will see something like so in the logs:

+ brew install bison
==> Fetching bison
==> Downloading https://ghcr.io/v2/homebrew/core/bison/manifests/3.8.2
Already downloaded: /Users/kbuilder/Library/Caches/Homebrew/downloads/0a84b14c20dfba4609542ea4b14a4eb93d369f7f83f373b568017fc7d76b6505--bison-3.8.2.bottle_manifest.json
==> Downloading https://ghcr.io/v2/homebrew/core/bison/blobs/sha256:a4fa1a0bf3245d8ef6a0d24d35df5222269174a02408784d870e4a882434712d
Already downloaded: /Users/kbuilder/Library/Caches/Homebrew/downloads/96839926e11a8ea1612145bf63e7eeb8b7862a2379fa1b2f8a3036532cd09f93--bison--3.8.2.big_sur.bottle.tar.gz
==> Pouring bison--3.8.2.big_sur.bottle.tar.gz
