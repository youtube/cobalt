FROM linux-x64x11-xenial

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends clang-3.6 \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

# This environment variable changes the output log line for Ninja build commands
# to also print the timing information.
ENV NINJA_STATUS "[%f/%t] (%es) "

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} && \
    ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
