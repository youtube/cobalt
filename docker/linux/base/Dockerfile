ARG BASE_OS
ARG BASE_OS_TAG
FROM ${BASE_OS:-gcr.io/cloud-marketplace-containers/google/debian9}:${BASE_OS_TAG:-latest}

ENV PYTHONUNBUFFERED 1

# === Install common dependencies
RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        python-pip \
        python-setuptools \
        python-wheel \
        python3-dev \
        python3-pip \
        python3-setuptools \
        git ccache \
        curl xz-utils \
        build-essential \
        libssl-dev \
        libcurl4-gnutls-dev \
        libexpat1-dev \
        gettext \
        unzip \
        zlib1g-dev \
    && curl -fsSL "https://github.com/git/git/archive/v2.18.0.tar.gz" -o git-2.18.0.tar.gz \
    && tar -zxf git-2.18.0.tar.gz \
    && cd git-2.18.0 \
    && make prefix=/usr/local all \
    && make prefix=/usr/local install \
    && cd .. \
    && rm -rf git-2.18.0 git-2.18.0.tar.gz \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD ["/usr/bin/python","--version"]
