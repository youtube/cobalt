FROM cobalt-build-linux-stub

CMD gn gen ${OUTDIR}/${PLATFORM}_${CONFIG} --args="target_platform=\"${PLATFORM}\" build_type=\"${CONFIG}\"" && \
    ninja -v -C ${OUTDIR}/${PLATFORM}_${CONFIG}
