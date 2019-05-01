# Compilation
Compile the dependencies first:

```$ make deps```

Then make everything:

```$ make```

# Running tests
To run tests do:

```$ make run_tests```

# Language specific details
This library has been created to run on a number of systems.  Specifically,
layers of indirection were built such that it is able to be run on systems such
as iOS (by simply replacing the wrapper classes) and on systems with
custom/non-openssl implementations (by providing alternative implementations of
the code in ```crypto_ops_openssl.cc```).
