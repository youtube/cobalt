FROM cobalt-build-linux

CMD gn gen ${OUTDIR}/${PLATFORM}_${CONFIG} --args="target_platform=\"${PLATFORM}\" build_type=\"${CONFIG}\"" && \
    ninja -j ${NINJA_PARALLEL} -C ${OUTDIR}/${PLATFORM}_${CONFIG}
