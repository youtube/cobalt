/*
 * Copyright (c) 2004, Richard Levitte <richard@levitte.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "starboard/directory.h"
#include "starboard/memory.h"
#include "starboard/system.h"

/* Definition of the underlying LP_DIR_CTX, through a couple typedefs.*/
struct LP_dir_context_st
{
    SbDirectory dir;
    char entry_name[SB_FILE_MAX_NAME];
};

const char *LP_find_file(LP_DIR_CTX **ctx, const char *directory)
{
    SbDirectoryEntry entry;
    if (ctx == NULL || directory == NULL) {
        return NULL;
    }

    SbSystemClearLastError();
    if (*ctx == NULL) {
        *ctx = (LP_DIR_CTX *)SbMemoryAllocate(sizeof(LP_DIR_CTX));
        if (*ctx == NULL) {
            return NULL;
        }
        SbMemorySet(*ctx, '\0', sizeof(LP_DIR_CTX));

        (*ctx)->dir = SbDirectoryOpen(directory, NULL);
        if (!SbDirectoryIsValid((*ctx)->dir)) {
            SbMemoryFree(*ctx);
            *ctx = NULL;
            return NULL;
        }
    }

    if (!SbDirectoryGetNext((*ctx)->dir, &entry)) {
        return NULL;
    }

    SbStringCopy((*ctx)->entry_name, entry.name, sizeof((*ctx)->entry_name));
    return (*ctx)->entry_name;
}

int LP_find_file_end(LP_DIR_CTX **ctx)
{
    if (ctx != NULL && *ctx != NULL) {
        bool result = SbDirectoryClose((*ctx)->dir);
        SbMemoryFree(*ctx);
        return (result ? 1 : 0);
    }
    return 0;
}
