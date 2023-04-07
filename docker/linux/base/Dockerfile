ARG BASE_OS
ARG BASE_OS_TAG
FROM ${BASE_OS:-gcr.io/cloud-marketplace-containers/google/debian10}:${BASE_OS_TAG:-latest}

ENV PYTHONUNBUFFERED 1

ARG GIT_SHA265SUM="93993babac690a06906d832a1715c3315b4787a2845aeb500f7dcc82e1599df2 git-2.18.0.tar.gz"

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
    && echo ${GIT_SHA265SUM} | sha256sum --check \
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
