# How To Use
A docker container will be launched that will update the cobalt.dev source files
in the Cobalt repository specified by the environment variable `COBALT_SRC` or
the current working directory, which should be the root of the Cobalt
repository, if that environment variable is not set.

When the `docsite` service is run, the container will generate the site and host
the updated cobalt.dev site locally on port `4000`.

When the `docsite-deploy` service is run, the container will build and
output the site content in the directory `out/deploy/www` at the root of the
Cobalt directory, and copy the app.yaml deployable into `out/deploy`.

# How To Run
docker-compose build --no-cache --build-arg USER=defaultuser --build-arg UID=$(id -u) --build-arg GID=$(id -g) SERVICE

docker-compose up SERVICE
