ARG BASE_OS
ARG BASE_OS_TAG
FROM ${BASE_OS:-marketplace.gcr.io/google/debian10}:${BASE_OS_TAG:-latest}

ENV PYTHONUNBUFFERED 1

# === Install common dependencies
RUN echo "deb http://archive.debian.org/debian buster main" > /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian-security buster/updates main" >> /etc/apt/sources.list

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

ARG GCLOUD=google-cloud-cli-412.0.0-linux-x86_64.tar.gz
RUN cd /tmp \
    && curl -L -O "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/${GCLOUD}" \
    && mkdir -p /opt/google-cloud-sdk \
    && tar xf "${GCLOUD}" -C /opt/google-cloud-sdk --strip-components=1 google-cloud-sdk/bin google-cloud-sdk/lib \
    && ln -s /opt/google-cloud-sdk/bin/gcloud /usr/local/bin/gcloud \
    && rm "${GCLOUD}" \
    && gcloud config set storage/check_hashes never \
    && gcloud --version

CMD ["/usr/bin/python","--version"]
