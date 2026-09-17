ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

# --- Stage 1: Build Git >= 2.43 from release source ---
FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA} AS git-builder

# Git >= 2.43 is required for --end-of-options in gclient sync (b/562551706)
ARG GIT_VERSION=2.46.2
ARG GIT_SHA256=65c5689fd44f1d09de7fd8c44de7fef074ddd69dda8b8503d44afb91495ecbce

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        libcurl4-openssl-dev \
        libexpat1-dev \
        libssl-dev \
        zlib1g-dev \
    && curl -fsSL "https://www.kernel.org/pub/software/scm/git/git-${GIT_VERSION}.tar.gz" -o git.tar.gz \
    && echo "${GIT_SHA256}  git.tar.gz" | sha256sum --check \
    && tar -zxf git.tar.gz \
    && cd "git-${GIT_VERSION}" \
    && make -j$(nproc) prefix=/usr/local DESTDIR=/opt/git NO_TCLTK=1 NO_GETTEXT=1 all install

# --- Stage 2: Clean Shared Base Container ---
FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Copy compiled Git binaries
COPY --from=git-builder /opt/git/usr/local /usr/local

# Install common base runtime utilities & UTF-8 locale in a single layer
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        python3-dev \
        xz-utils \
        zstd \
        jq \
        pkgconf \
        libglib2.0-0 \
        libnss3 \
        libasound2 \
        libcurl4 \
        libexpat1 \
        locales \
    && sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
    && locale-gen \
    && rm -rf /var/lib/apt/lists/*

# This locale satisfies SbSystemGetLocaleIdTest and standard UTF-8 environments
ENV LANG=en_US.UTF-8

# Install Google Cloud SDK
ARG GCLOUD=google-cloud-cli-linux-x86_64.tar.gz
RUN cd /tmp \
    && curl -fsSL -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
    && tar xf "${GCLOUD}" -C /usr/local --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
    && rm "${GCLOUD}" \
    && gcloud --version
