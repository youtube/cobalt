Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `directory.h`

Provides directory listing functions.

## Macros

### kSbDirectoryInvalid

Well-defined value for an invalid directory stream handle.

## Typedefs

### SbDirectory

A handle to an open directory stream.

#### Definition

```
typedef struct SbDirectoryPrivate* SbDirectory
```

## Functions

### SbDirectoryCanOpen

Indicates whether SbDirectoryOpen is allowed for the given `path`.

`path`: The path to be checked.

#### Declaration

```
bool SbDirectoryCanOpen(const char *path)
```

### SbDirectoryClose

Closes an open directory stream handle. The return value indicates whether the
directory was closed successfully.

`directory`: The directory stream handle to close.

#### Declaration

```
bool SbDirectoryClose(SbDirectory directory)
```

### SbDirectoryCreate

Creates the directory `path`, assuming the parent directory already exists. This
function returns `true` if the directory now exists (even if it existed before)
and returns `false` if the directory does not exist.

`path`: The path to be created.

#### Declaration

```
bool SbDirectoryCreate(const char *path)
```

### SbDirectoryGetNext

Populates `out_entry` with the next entry in the specified directory stream, and
moves the stream forward by one entry.

Platforms may, but are not required to, return `.` (referring to the directory
itself) and/or `..` (referring to the directory's parent directory) as entries
in the directory stream.

This function returns `true` if there was a next directory, and `false` at the
end of the directory stream or if `out_entry_size` is smaller than
kSbFileMaxName.

`directory`: The directory stream from which to retrieve the next directory.
`out_entry`: The null terminated string to be populated with the next directory
entry. The space allocated for this string should be equal to `out_entry_size`.
`out_entry_size`: The size of the space allocated for `out_entry`. This should
be at least equal to kSbFileMaxName.

#### Declaration

```
bool SbDirectoryGetNext(SbDirectory directory, char *out_entry, size_t out_entry_size)
```

### SbDirectoryIsValid

Returns whether the given directory stream handle is valid.

#### Declaration

```
static bool SbDirectoryIsValid(SbDirectory directory)
```

### SbDirectoryOpen

Opens the given existing directory for listing. This function returns
kSbDirectoryInvalidHandle if it is not successful.

If `out_error` is provided by the caller, it will be set to the appropriate
SbFileError code on failure.

`out_error`: An output parameter that, in case of an error, is set to the reason
that the directory could not be opened.

#### Declaration

```
SbDirectory SbDirectoryOpen(const char *path, SbFileError *out_error)
```
