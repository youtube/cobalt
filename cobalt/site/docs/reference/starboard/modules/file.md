Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `file.h`

Defines file system input/output functions.

## Functions

### SbFileAtomicReplace

Replaces the content of the file at `path` with `data`. Returns whether the
contents of the file were replaced. The replacement of the content is an atomic
operation. The file will either have all of the data, or none.

`path`: The path to the file whose contents should be replaced. `data`: The data
to replace the file contents with. `data_size`: The amount of `data`, in bytes,
to be written to the file.

#### Declaration

```
bool SbFileAtomicReplace(const char *path, const char *data, int64_t data_size)
```
