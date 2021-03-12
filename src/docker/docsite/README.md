# How It Works
A Docker container will be launched that will update the cobalt.dev source files
in the Cobalt repository specified by `$COBALT_SRC`, which must be set, and it
will locally host the updated cobalt.dev on port `4000`.

A separate docker-compose.yml was created due to the root docker-compose.yml
being unable to locate the Gemfile in the Docker build context.

The Gemfile was copied from third_party/repo-publishing-toolkit/Gemfile because
we need to run `bundle install` with elevated permissions but if we let the
`preview-site.sh`, which normally installs gems, run with elevated permissions
the resulting files added or modified would not be accessible to the user who
ran the Docker container.

# How To Run
docker-compose build --build-arg UID=$(id -u) --build-arg GID=$(id -g) docsite
docker-compose up docsite
