kimono_snapshot.json lists files/dirs that Kimono build requires.
The file list was generated from snapshot-chrobalt-android.sh on google3.
All file/dir path are relative to out/android-#ARC_#BUILD. Each architecture/build requires its own snapshot.
The listed dirs contain more than what's needed by today's Kimono build because future build rule changes might add/remove Java dependencies easily. Copying entire dirs allow us to avoid changing the snapshot process frequently.
There are additional file move and modification operations needed during/after storing them in google3.
