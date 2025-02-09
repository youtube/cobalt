# Use the official Ubuntu image as the base
FROM debian:12

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
   libasound2 \
   libdbus-1-3 \
   libgbm-dev \
   libnss3 \
   libxcomposite-dev \
   # xauth \
    xvfb \
    jq \
    && rm -rf /var/lib/apt/lists/*
