FROM build-base-windows

RUN git clone https://cobalt.googlesource.com/depot_tools

ADD win32/build.cmd /build.cmd

CMD [ "/build.cmd" ]
