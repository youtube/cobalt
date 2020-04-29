FROM base

# TODO don't need full clang install here -
# workout more minimal toolchain management.
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

RUN git clone https://cobalt.googlesource.com/depot_tools /depot_tools

ENV PATH="${PATH}:/depot_tools:/root/fake_goma" \
    DEPOT_TOOLS_UPDATE=0 \
    CCACHE_DIR=/root/ccache \
    CCACHE_MAXSIZE=30G

COPY ./files/fake_goma /root/fake_goma

# Pull in gclient deps
RUN cd /tmp && gclient verify || true \
    && chmod +x /root/fake_goma/gomacc /root/fake_goma/goma_ctl.py \
    && mkdir /root/ccache

WORKDIR /code
CMD ["/scripts/build_cobalt.py"]
