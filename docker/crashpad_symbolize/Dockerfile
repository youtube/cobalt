FROM cobalt-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        jq build-essential zlib1g-dev libzstd-dev \
    && /opt/clean-after-apt.sh

RUN cd /tmp \
  && git clone 'https://chromium.googlesource.com/chromium/tools/depot_tools.git'

ENV PATH=${PATH}:/tmp/depot_tools

RUN mkdir breakpad \
  && cd breakpad \
  && fetch breakpad \
  && cd src \
  && ./configure \
  && make

ARG GITHUB_TAG
RUN curl -L \
  -H "Accept: application/vnd.github+json" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  https://api.github.com/repos/youtube/cobalt/releases/tags/${GITHUB_TAG} \
  > /tmp/release.json

ARG ARCHITECTURE
ARG SB_API_VERSION
ARG CONFIG
ENV CONTAINS_STRING unstripped_${ARCHITECTURE}_sbversion-${SB_API_VERSION}_${CONFIG}
RUN jq -r --arg str "$CONTAINS_STRING" '.assets[] | select(.name | contains($str)) | .id' /tmp/release.json > /tmp/asset_ids.txt \
  && rm /tmp/release.json

RUN while read asset_id; do \
      curl -LJO \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        https://api.github.com/repos/youtube/cobalt/releases/assets/${asset_id}; \
  done < /tmp/asset_ids.txt

RUN id=$(cat /tmp/asset_ids.txt) && \
    rm /tmp/asset_ids.txt && \
    curl -s -L \
      -H "Accept: application/octet-stream" \
      -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/youtube/cobalt/releases/assets/$id -o /tmp/libcobalt.tgz

RUN mkdir /tmp/cobalt-evergreen-snapshot/ && \
    tar xzf /tmp/libcobalt.tgz -C /tmp/cobalt-evergreen-snapshot

RUN libcobalt_path=$(find /tmp/cobalt-evergreen-snapshot/ -name "libcobalt.so") \
  && breakpad/src/src/tools/linux/dump_syms/dump_syms $libcobalt_path > /tmp/libcobalt.so.sym \
  && debug_id=$(head -n1 /tmp/libcobalt.so.sym | cut -d' ' -f4) \
  && mkdir -p /tmp/symbols/libcobalt.so/$debug_id/ \
  && mv /tmp/libcobalt.so.sym /tmp/symbols/libcobalt.so/$debug_id/

CMD breakpad/src/src/processor/minidump_stackwalk /root/minidump_directory/minidump.dmp /tmp/symbols
