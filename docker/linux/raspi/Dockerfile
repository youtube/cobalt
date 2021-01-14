FROM cobalt-build-base

ARG raspi_home=/root/raspi-home
ARG raspi_sysroot=raspbian_lite_2017-07-05_sysroot.tar.xz

# Required by the gyp build system.
ENV RASPI_HOME=${raspi_home}

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        binutils \
        bison \
        g++-multilib \
        ninja-build \
        pkgconf \
        python-requests \
        wget \
        xz-utils \
        yasm \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

RUN mkdir -p ${raspi_home}/tools \
    && git clone https://github.com/raspberrypi/tools.git ${raspi_home}/tools

RUN cd ${raspi_home} \
    && wget -q \
    "https://storage.googleapis.com/cobalt-static-storage/${raspi_sysroot}"

RUN cd ${raspi_home} && tar Jxpvf ${raspi_sysroot}

CMD /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} \
    && ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
