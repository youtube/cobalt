ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install Git from release (Git >= 2.43 is required for --end-of-options in gclient sync, b/562551706)
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
    && make -j$(nproc) prefix=/usr/local NO_TCLTK=1 NO_GETTEXT=1 all install \
    && cd .. \
    && rm -rf "git-${GIT_VERSION}" git.tar.gz \
    && apt-get purge -y build-essential libcurl4-openssl-dev libexpat1-dev libssl-dev \
    && apt-get install -y --no-install-recommends libcurl4 libexpat1 \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/*

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    curl \
    dbus-x11 \
    ffmpeg \
    jq \
    libasound2 \
    libatomic1 \
    libdbus-1-3 \
    libegl1 \
    libegl1-mesa \
    libegl-mesa0 \
    libgbm-dev \
    libglapi-mesa \
    libgles2-mesa \
    libglib2.0-0 \
    libnss3 \
    libxcomposite-dev \
    openbox \
    python3 \
    python3-dev \
    x11-xserver-utils \
    xcompmgr \
    xcvt \
    xserver-xorg-core \
    xserver-xorg-video-dummy \
    xvfb \
    xz-utils \
    zstd \
    && rm -rf /var/lib/apt/lists/*

ARG GCLOUD=google-cloud-cli-linux-x86_64.tar.gz
RUN cd /tmp \
    && curl -L -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
    && tar xf "${GCLOUD}" -C /usr/local --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
    && rm "${GCLOUD}" \
    && gcloud --version

# This locale satisfies SbSystemGetLocaleIdTest.
ENV LANG=en_US.UTF-8
