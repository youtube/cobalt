FROM cobalt-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        libasound2 \
        libatomic1 \
        libavcodec-dev \
        libavformat-dev \
        libgles2-mesa \
        libegl1-mesa \
        libgl1-mesa-dri \
        libx11-6 \
        libxcomposite1 \
        libxi-dev \
        libxrender1 \
        procps \
        unzip \
        xauth \
        xvfb \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

RUN python2 -m pip install "selenium==3.141.0" "Brotli==1.0.9" six

WORKDIR /out

# Sets the locale in the environment. This is needed for NPLB unit tests.
ENV LANG en_US.UTF-8

RUN mkdir -p /app_launcher_out

CMD unzip -q /out/app_launcher -d /app_launcher_out && \
    xvfb-run --server-args="-screen 0 1920x1080x24 +render +extension GLX -noreset" \
    python /app_launcher_out/starboard/tools/testing/test_runner.py --run \
           -o /out --platform $PLATFORM --config $CONFIG
