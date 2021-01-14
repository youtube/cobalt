FROM cobalt-build-base

RUN apt update -qqy \
    && apt install -qqy --no-install-recommends \
        binutils \
        bison \
        default-jdk \
        g++-multilib \
        ninja-build \
        pkgconf \
        python-requests \
        yasm \
    && apt-get clean autoclean \
    && apt-get autoremove -y --purge \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && rm -rf /var/lib/{apt,dpkg,cache,log} \
    && echo "Done"

RUN mkdir -p /root/.android

CMD (test -f /root/.android/debug.keystore \
    && echo "Android debug keystore exists." \
    || (keytool -genkey -v \
               -keystore /root/.android/debug.keystore \
               -dname "cn=Android Docker, ou=YouTube, o=Google, c=US" \
               -storepass android \
               -alias androiddebugkey \
               -keypass android \
               -keyalg RSA \
               -keysize 2048 \
               -validity 10000 \
    && echo "Generated Android Debug keystore.")) \
    && /code/cobalt/build/gyp_cobalt -v -C ${CONFIG} ${PLATFORM} \
    && ninja -v -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG} ${TARGET:-cobalt_deploy}
