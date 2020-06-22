---
layout: doc
title: "Starboard Module Reference: character.h"
---

Provides functions for interacting with characters.

## Functions ##

### SbCharacterIsAlphanumeric ###

Indicates whether the given 8-bit character `c` (as an int) is alphanumeric in
the current locale.

`c`: The character to be evaluated.

#### Declaration ####

```
bool SbCharacterIsAlphanumeric(int c)
```

### SbCharacterIsDigit ###

Indicates whether the given 8-bit character `c` (as an int) is a decimal digit
in the current locale.

`c`: The character to be evaluated.

#### Declaration ####

```
bool SbCharacterIsDigit(int c)
```

### SbCharacterIsHexDigit ###

Indicates whether the given 8-bit character `c` (as an int) is a hexadecimal in
the current locale.

`c`: The character to be evaluated.

#### Declaration ####

```
bool SbCharacterIsHexDigit(int c)
```

### SbCharacterIsSpace ###

Indicates whether the given 8-bit character `c` (as an int) is a space in the
current locale.

`c`: The character to be evaluated.

#### Declaration ####

```
bool SbCharacterIsSpace(int c)
```

### SbCharacterIsUpper ###

Indicates whether the given 8-bit character `c` (as an int) is uppercase in the
current locale.

`c`: The character to be evaluated.

#### Declaration ####

```
bool SbCharacterIsUpper(int c)
```

### SbCharacterToLower ###

Converts the given 8-bit character (as an int) to lowercase in the current
locale and returns an 8-bit character. If there is no lowercase version of the
character, or the character is already lowercase, the function just returns the
character as-is.

`c`: The character to be converted.

#### Declaration ####

```
int SbCharacterToLower(int c)
```

### SbCharacterToUpper ###

Converts the given 8-bit character (as an int) to uppercase in the current
locale and returns an 8-bit character. If there is no uppercase version of the
character, or the character is already uppercase, the function just returns the
character as-is.

`c`: The character to be converted.

#### Declaration ####

```
int SbCharacterToUpper(int c)
```

