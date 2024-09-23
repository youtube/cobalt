ARG BASE_OS
ARG BASE_OS_TAG
FROM ${BASE_OS:-marketplace.gcr.io/google/debian10}:${BASE_OS_TAG:-latest}

ENV PYTHONUNBUFFERED 1

# === Install common dependencies
RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        python-pip python-setuptools python-wheel \
        python3-pip python3-requests \
        git ccache \
        curl xz-utils\
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD ["/usr/bin/python","--version"]
