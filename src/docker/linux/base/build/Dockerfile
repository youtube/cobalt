ARG FROM_IMAGE
FROM ${FROM_IMAGE:-cobalt-base}

# === Get Nodejs pinned LTS version via NVM
ENV NVM_DIR /root/.nvm
ENV NODE_VERSION 12.17.0

RUN curl --silent -o- https://raw.githubusercontent.com/creationix/nvm/v0.35.3/install.sh | bash

RUN . $NVM_DIR/nvm.sh \
   && nvm install --lts \
   && nvm alias default lts/* \
   && nvm use default

ENV NODE_PATH $NVM_DIR/v$NODE_VERSION/lib/node_modules
ENV PATH $NVM_DIR/versions/node/v$NODE_VERSION/bin:$PATH


# === Configure common env vars
ENV OUTDIR=out \
    NINJA_STATUS="[%f/%t %c/sec] " \
    NINJA_PARALLEL=4 \
    CCACHE_DIR=/root/ccache \
    CCACHE_MAXSIZE=30G

# == Set up  ccache
RUN cd /tmp && mkdir /root/ccache

WORKDIR /code
CMD ["/usr/bin/python","--version"]
