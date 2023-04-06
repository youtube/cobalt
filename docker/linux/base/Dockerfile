ARG BASE_OS
ARG BASE_OS_TAG
ARG NEEDS_GIT_UPDATE
FROM ${BASE_OS:-gcr.io/cloud-marketplace-containers/google/debian9}:${BASE_OS_TAG:-latest} as stage1

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
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

FROM stage1 as git-update-no
# Nothing to be done

FROM stage1 as git-update-yes
# Install git PPA and update
RUN apt update -qqy \
    && add-apt-repository -y ppa:git-core/ppa \
    && apt-get -y install git \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

FROM git-update-${NEEDS_GIT_UPDATE}
RUN git --version

RUN echo ${gitup}
CMD ["/usr/bin/python","--version"]
