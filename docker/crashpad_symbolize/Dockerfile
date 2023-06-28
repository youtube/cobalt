FROM cobalt-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends jq build-essential \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log}

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
RUN jq -r '.assets[] | select(.name | contains($CONTAINS_STRING)) | .id' /tmp/release.json > /tmp/asset_id.txt \
  && rm /tmp/release.json

RUN while read asset_id; do \
      curl -LJO \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        https://api.github.com/repos/youtube/cobalt/releases/assets/${asset_id}; \
  done < asset_ids.txt \
  && rm /tmp/asset_ids.txt

RUN id=$(cat asset_ids.txt) && \
    curl -s -L \
      -H "Accept: application/vnd.github.v3+json" \
      -H "X-GitHub-Api-Version: 2022-11-28" \
    https://api.github.com/repos/$owner/$repo/releases/assets/$id -o /tmp/libcobalt.tgz

RUN tar xzf /tmp/libcobalt.tgz
RUN libcobalt_path=$(find /tmp/tmp/cobalt-evergreen-snapshot/ -name "libcobalt.so") \
  && /tmp/src/tools/linux/dump_syms/dump_syms $libcobalt_path > /tmp/libcobalt.so.sym \
  && debug_id=$(head -n1 /tmp/libcobalt.so.sym | cut -d' ' -f4) \
  && mkdir -p /tmp/symbols/libcobalt.so/$debug_id/ \
  && mv /tmp/libcobalt.so.sym /tmp/symbols/libcobalt.so/$debug_id/

CMD /tmp/src/processor/minidump_stackwalk ${MINIDUMP_PATH} /tmp/symbols
