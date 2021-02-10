FROM build-base-windows

RUN git clone https://cobalt.googlesource.com/depot_tools
RUN setx path "%path%;C:\depot_tools"

# This is necessary in order to ensure that gyp configures the build without PDB
# generation.
# PDB files cannot be created inside of volumes due to limitations with windows
# docker engine and our current in-source build.
RUN setx is_docker 1

ADD scripts/build.cmd /build.cmd
ADD scripts/fast-win-rmdir.cmd /fast-win-rmdir.cmd

CMD [ "/build.cmd", "${PLATFORM}", "${CONFIG}", "${TARGET}", "${NINJA_FLAGS}" ]
