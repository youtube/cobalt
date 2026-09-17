ARG BASE_IMAGE=base
FROM ${BASE_IMAGE}

# Install any necessary dependencies for unit tests and virtual display
RUN apt-get update && apt-get install -y --no-install-recommends \
    dbus-x11 \
    ffmpeg \
    libatomic1 \
    libdbus-1-3 \
    libegl1 \
    libegl1-mesa \
    libegl-mesa0 \
    libgbm-dev \
    libglapi-mesa \
    libgles2-mesa \
    libxcomposite-dev \
    openbox \
    python3 \
    x11-xserver-utils \
    xcompmgr \
    xcvt \
    xserver-xorg-core \
    xserver-xorg-video-dummy \
    xvfb \
    && rm -rf /var/lib/apt/lists/*
