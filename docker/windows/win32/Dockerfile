FROM build-base-windows

RUN git clone https://cobalt.googlesource.com/depot_tools

ADD scripts/build.cmd /build.cmd

CMD [ "/build.cmd", "${PLATFORM}", "${CONFIG}", "${TARGET}", "${NINJA_FLAGS}" ]
