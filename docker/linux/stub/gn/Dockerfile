FROM cobalt-build-linux-stub

CMD gn gen ${OUTDIR}/${PLATFORM}_${CONFIG} --args="target_os=\"${PLATFORM}\" build_type=\"${CONFIG}\"" && \
    ninja -v -C ${OUTDIR}/${PLATFORM}_${CONFIG}
