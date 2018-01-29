/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// TODO: add unittests for all these operations

#ifndef SkOSFile_DEFINED
#define SkOSFile_DEFINED

#if defined(STARBOARD)
#include "starboard/file.h"
#define SkFile SbFilePrivate
#else
#include <stdio.h>
#define SkFile FILE
#endif

#include "SkString.h"

enum SkFILE_Flags {
    kRead_SkFILE_Flag   = 0x01,
    kWrite_SkFILE_Flag  = 0x02
};

SkFile* sk_fopen(const char path[], SkFILE_Flags);
void    sk_fclose(SkFile*);

size_t  sk_fgetsize(SkFile*);

size_t  sk_fwrite(const void* buffer, size_t byteCount, SkFile*);

void    sk_fflush(SkFile*);
void    sk_fsync(SkFile*);

size_t  sk_ftell(SkFile*);

/** Maps a file into memory. Returns the address and length on success, NULL otherwise.
 *  The mapping is read only.
 *  When finished with the mapping, free the returned pointer with sk_fmunmap.
 */
void*   sk_fmmap(SkFile* f, size_t* length);

/** Maps a file descriptor into memory. Returns the address and length on success, NULL otherwise.
 *  The mapping is read only.
 *  When finished with the mapping, free the returned pointer with sk_fmunmap.
 */
void*   sk_fdmmap(int fd, size_t* length);

/** Unmaps a file previously mapped by sk_fmmap or sk_fdmmap.
 *  The length parameter must be the same as returned from sk_fmmap.
 */
void    sk_fmunmap(const void* addr, size_t length);

/** Returns true if the two point at the exact same filesystem object. */
bool    sk_fidentical(SkFile* a, SkFile* b);

/** Returns the underlying file descriptor for the given file.
 *  The return value will be < 0 on failure.
 */
int     sk_fileno(SkFile* f);

/** Returns true if something (file, directory, ???) exists at this path,
 *  and has the specified access flags.
 */
bool    sk_exists(const char *path, SkFILE_Flags = (SkFILE_Flags)0);

// Returns true if a directory exists at this path.
bool    sk_isdir(const char *path);

// Like pread, but may affect the file position marker.
// Returns the number of bytes read or SIZE_MAX if failed.
size_t sk_qread(SkFile*, void* buffer, size_t count, size_t offset);


// Create a new directory at this path; returns true if successful.
// If the directory already existed, this will return true.
// Description of the error, if any, will be written to stderr.
bool    sk_mkdir(const char* path);

class SkOSFile {
public:
    class Iter {
    public:
        Iter();
        Iter(const char path[], const char suffix[] = NULL);
        ~Iter();

        void reset(const char path[], const char suffix[] = NULL);
        /** If getDir is true, only returns directories.
            Results are undefined if true and false calls are
            interleaved on a single iterator.
        */
        bool next(SkString* name, bool getDir = false);

        static const size_t kStorageSize = 40;
    private:
        SkAlignedSStorage<kStorageSize> fSelf;
    };
};

#endif
