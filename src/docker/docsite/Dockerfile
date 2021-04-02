FROM gcr.io/cloud-marketplace-containers/google/ruby

ARG UID
ARG GID

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends bundler doxygen git nodejs python \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log}

COPY Gemfile /app/Gemfile

# Install and use the latest Ruby version, and install all of the required gems.
RUN rbenv install 2.7.1 && rbenv global 2.7.1 \
    && bundle install \
    && echo "Done"

# We create and use a non-root user explicitly so that the generated and
# modified files maintain the same permissions as the user that launched the
# Docker container.
RUN addgroup --group --gid "${GID}" defaultgroup \
    && adduser --disabled-password --gecos '' --uid "${UID}" --gid "${GID}" defaultuser

USER defaultuser

CMD /cobalt/third_party/repo-publishing-toolkit-local/preview-site.sh
