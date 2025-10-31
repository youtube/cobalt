#if defined(STARBOARD)

// Undefining ioctl operations here to allow definition of the implementation
// as ioctl_FOO by name instead of by number.
#undef TIOCGWINSZ

#endif
