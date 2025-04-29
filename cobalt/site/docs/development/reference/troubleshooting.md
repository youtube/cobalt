Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Troubleshooting

## Download failed in download_from_gcs.py with "HTTP Error 403: Forbidden"

### What Are the Issues / Symptoms?

After Apr 24, 2025, local Cobalt builds may break due to new Google compliance requirements.

**`403` access errors when attempting to fetch files from the original static storage endpoint.**

For example:

```
python3 ../../starboard/tools/download_from_gcs.py --bucket cobalt-static-storage --sha1 ../../starboard/shared/starboard/player/testdata --output content/data/test/starboard/shared/starboard/player/testdata
[ERROR:download_from_gcs.py:92] Download failed: 'HTTP Error 403: Forbidden', retrying
Traceback (most recent call last):
  File "/__w/cobalt/cobalt/src/out/android-arm_gold/../../starboard/tools/download_from_gcs.py", line 90, in MaybeDownloadFileFromGcs
    tmp_file = _DownloadFromGcsAndCheckSha1(bucket, sha1)
               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
```

### How Urgent Is This?

P0 - Your local Cobalt builds may stop working on Apr 24, 2025.

### What Builds/Branches Are Affected?

All Cobalt builds in a fresh repo checkout are susceptible to this failure since they require Google Cloud Storage(GCS) bucket access to download build dependencies, test artifacts, and toolchains. Cobalt branches 25 and older are affected by this.

### How Do I Fix This?

Partners MUST switch to using the new public versions of Google Static Storage Endpoints.

You can fix this by search-and-replace all instances in your code/builds where you see:

* `cobalt-static-storage` → `cobalt-static-storage-public`
* `lottie-coverage-testdata` → `lottie-coverage-testdata-public`

Alternatively, you can use sed from the top-level directory of the repository
checkout to fix all the files locally with these commands:

```
sed -i "s/cobalt-static-storage/cobalt-static-storage-public/g" *
sed -i "s/lottie-coverage-testdata/lottie-coverage-testdata-public/g" *
```

Here is also a set of example GitHub PRs that has changes to address this:

* 25 LTS: [Ensure GCS buckets are publicly accessible mirrors
#5255](https://github.com/youtube/cobalt/pull/5255/files)
* 24 LTS: [Ensure GCS buckets are publicly accessible mirrors
#5331](https://github.com/youtube/cobalt/pull/5331/files)
* 23 LTS: [Ensure GCS buckets are publicly accessible mirrors
#5332](https://github.com/youtube/cobalt/pull/5332/files)

Git commands are also available to apply the changes

```
# Update latest changes from server
git fetch

# Cobalt 25.lts tags and branches
git cherry-pick 6697fc46e6

# Cobalt 24.lts tags and branches
git cherry-pick 43d272d35a

# Cobalt 23.lts tags and brancees
git cherry-pick 2b0e3b9d37
```

### What If the Fix Still Doesn’t Work?

Please file a ticket and report to your assigned YouTube Technical Account Manager.
