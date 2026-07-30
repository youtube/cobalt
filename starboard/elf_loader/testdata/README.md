# ELF Loader Test Data

This directory contains test data files used by the Starboard ELF Loader unit tests (`lz4_file_impl_test.cc` and `naive_zstd_file_impl_test.cc`).

## Source File

*   `uncompressed`: A text file containing 64 bytes of `'a'` followed by a newline (65 bytes total).

## LZ4 Compressed Files

*   `compressed.lz4`: Single-frame LZ4 compressed file with content size in the frame header.
*   `compressed_no_content_size.lz4`: Single-frame LZ4 compressed file without content size in the frame header.

## Zstandard (Zstd) Compressed Files

<!-- TODO: b/497012299 - To ensure decompression compatibility, use the Zstd
source code from the repository to generate these test data files. -->

To regenerate the Zstandard test data, use the standard `zstd` command line tool:

*   `compressed.zst`: Single-frame Zstd compressed file with content size in the frame header (written by default).
    ```bash
    zstd -3 uncompressed -o compressed.zst
    ```

*   `compressed_no_content_size.zst`: Single-frame Zstd compressed file without content size in the frame header.
    ```bash
    zstd -3 --no-content-size uncompressed -o compressed_no_content_size.zst
    ```

*   `compressed_multi_frame.zst`: Multi-frame Zstd compressed file containing exactly two frames.
    ```bash
    split -n 2 -d uncompressed chunk_
    zstd -3 chunk_00 -o chunk_00.zst
    zstd -3 chunk_01 -o chunk_01.zst
    cat chunk_00.zst chunk_01.zst > compressed_multi_frame.zst
    rm chunk_00 chunk_01 chunk_00.zst chunk_01.zst
    ```

*   `empty.zst`: An empty file (0 bytes).
    ```bash
    touch empty.zst
    ```

*   `compressed_with_garbage.zst`: A Zstd compressed file with trailing garbage data.
    ```bash
    cp compressed.zst compressed_with_garbage.zst
    echo "garbage_data" >> compressed_with_garbage.zst
    ```
