ARG FROM_IMAGE
FROM ${FROM_IMAGE:-cobalt-build-base}

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        pkgconf ninja-build bison yasm \
        binutils \
        clang \
        libgles2-mesa-dev \
        mesa-common-dev \
        libpulse-dev \
        libavformat-dev \
        libavresample-dev \
        libasound2-dev \
        libxrender-dev \
        libxcomposite-dev \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} && \
    ninja -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
