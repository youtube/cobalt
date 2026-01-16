# Specify the Base Image
FROM ubuntu:latest

# Install base utilities, Android test dependencies (adb, netcat),
# and Linux test dependencies (GUI/X11 libraries).
RUN apt-get update && apt-get install -y \
    adb \
    netcat-openbsd \
    python3 \
    ca-certificates \
    xvfb \
    openbox \
    libnss3 \
    libasound2t64 \
    libxcomposite1 \
    libxdamage1 \
    libxrandr2 \
    libgbm1 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /opt/cobalt_browsertests

# Add and extract the artifacts tarball.
# Expects cobalt_browsertests_artifacts.tar.gz to be in the build context.
ADD cobalt_browsertests_artifacts.tar.gz .

# Start a bash shell by default.
CMD ["bash"]
