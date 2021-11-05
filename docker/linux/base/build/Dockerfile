ARG FROM_IMAGE
FROM ${FROM_IMAGE:-cobalt-base}

# === Get Nodejs pinned LTS version via NVM
ARG NVM_SHA256SUM="f068e17dacb88f73302790cc076956c7a0d459ce9b01df842ff3e75744f9e2fe /tmp/install.sh"
ARG NVM_URL="https://cobalt.googlesource.com/third_party/nvm/+/refs/tags/v0.35.3/install.sh?format=TEXT"
ENV NVM_DIR /root/.nvm
ENV NODE_VERSION 12.17.0

RUN curl --silent -o- ${NVM_URL} \
  | base64 -d > /tmp/install.sh \
   && echo ${NVM_SHA256SUM} | sha256sum --check \
   && . /tmp/install.sh

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
    NINJA_STATUS="[%e sec | %f/%t %u remaining | %c/sec | j%r] " \
    NINJA_PARALLEL=4 \
    IS_CI=0 \
    CCACHE_DIR=/root/ccache \
    CCACHE_MAXSIZE=30G

# == Set up gclient and ccache
RUN cd /tmp && gclient verify || true \
    && mkdir /root/ccache

WORKDIR /code
CMD ["/usr/bin/python","--version"]
