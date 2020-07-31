ARG BASE_OS
FROM ${BASE_OS:-gcr.io/cloud-marketplace-containers/google/debian9}

ENV PYTHONUNBUFFERED 1

# === Install common dependencies
RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        python-pip python-setuptools python-wheel \
        git ccache \
        curl \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD ["/usr/bin/python","--version"]
