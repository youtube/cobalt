Ensure you have a COBALT_SRC environment variable set up,
pointing to your local copy of Cobalt. e.g. ~/cobalt/src

Then choose your target, e.g. stub, linux-x64x11, etc..

then you can build that for that platform via:

docker-compose run <target>

By default, a debug build will be build. You can override this with an env
variable.

e.g. docker-compose run -e CONFIG=devel <target>
where config is one of the four optimization levels, debug, devel, qa and gold.
See https://cobalt.googlesource.com/cobalt/+/master/src/README.md

Builds will be available in your ${COBALT_SRC}/out dir

