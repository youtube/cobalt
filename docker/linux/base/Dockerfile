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
        software-properties-common \
        gnupg dirmngr \
    && apt-key adv --keyserver keyserver.ubuntu.com \
        --recv-keys A1715D88E1DF1F24 \
    && add-apt-repository -y ppa:git-core/ppa \
    && apt update -qqy \
    && apt install -qqy --no-install-recommends git \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD ["/usr/bin/python","--version"]
