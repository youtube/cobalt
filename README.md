# libiamf

This is a reference implementation and conformance test for the AOM Immersive Audio Model and Formats (IAMF) specification, approved as a final deliverable by AOM on April 3, 2024.
The reference implementation includes only the decoder. The description of each folder is as follows:

- The 'code' directory contains the reference decoder for AOM IAMF v1.0.0-errata.
- The 'proto' and 'tests' directories contain test vectors and test description metadata for conformance tests.

The source code within this repository that implements the normative portions of the IAMF specification is intended to form the IAMF Reference Implementation approved as an AOM Final Deliverable. Any additional functionality within this repository, including functionality that implements non-normative portions of the IAMF specification, is NOT intended to form part of this IAMF Reference Implementation.

# Changes since v1.0.0 release

- <a href="https://github.com/AOMediaCodec/libiamf/pull/82">MP4 Update according to https://github.com/AOMediaCodec/iamf/pull/794.</a>
- <a href="https://github.com/AOMediaCodec/libiamf/pull/81">Decoder Update according to https://github.com/AOMediaCodec/iamf/pull/794 and https://github.com/AOMediaCodec/iamf/pull/795.</a>
- <a href="https://github.com/AOMediaCodec/libiamf/pull/76">Add CONTRIBUTING.md for contributor agreement.</a>
- <a href="https://github.com/AOMediaCodec/libiamf/pull/72">Remove vestigial sync.proto.</a>
