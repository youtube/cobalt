Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `thread.h`

Defines functionality related to thread creation and cleanup.

## Macros

### kSbThreadInvalidId

Well-defined constant value to mean "no thread ID."


## Typedefs

### SbThreadId

An ID type that is unique per thread.

#### Definition

```
typedef int32_t SbThreadId
```

## Functions


### SbThreadIsValidId

Returns whether the given thread ID is valid.

#### Declaration

```
static bool SbThreadIsValidId(SbThreadId id)
```
