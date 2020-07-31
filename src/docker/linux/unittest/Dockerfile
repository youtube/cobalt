FROM cobalt-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libx11-6 \
        libxcomposite1 \
        libxrender1 \
        libasound2 \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

WORKDIR /out

CMD ./${TEST_TARGET}
