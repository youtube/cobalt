FROM cobalt-build-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        pkgconf ninja-build bison yasm \
        binutils \
        clang \
        libxml2 \
        binutils-aarch64-linux-gnu \
        binutils-arm-linux-gnueabi \
        g++-multilib \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG:-debug} ${PLATFORM} && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG:-debug} ${TARGET:-cobalt_deploy}
