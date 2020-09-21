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

# === Install depot_tools
RUN git clone https://cobalt.googlesource.com/depot_tools /depot_tools

# === Configure common env vars
ENV PATH="${PATH}:/depot_tools" \
    OUTDIR=out \
    DEPOT_TOOLS_UPDATE=0 \
    NINJA_STATUS="[%f/%t %c/sec] " \
    CCACHE_DIR=/root/ccache \
    CCACHE_MAXSIZE=30G

# == Set up gclient and ccache
RUN cd /tmp && gclient verify || true \
    && mkdir /root/ccache

WORKDIR /code
CMD ["/usr/bin/python","--version"]
