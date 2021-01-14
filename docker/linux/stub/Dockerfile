FROM cobalt-build-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        pkgconf ninja-build bison yasm \
        binutils \
        libstdc++-6-dev \
        mesa-common-dev \
        libx11-dev \
        libxcomposite-dev \
        libxrender-dev \
        libasound2-dev \
        libxml2 \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
