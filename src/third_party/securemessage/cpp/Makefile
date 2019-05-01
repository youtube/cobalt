# Copyright 2014 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PROTOBUF_DIR=third-party/google/protobuf/protobuf-2.5.0/
PROTOBUF_HDRS=third-party/google/protobuf/include/
PROTOBUF_LIBS=third-party/google/protobuf/lib/
PROTOC=third-party/google/protobuf/bin/protoc
GTEST_DIR=third-party/google/gtest/gtest-1.7.0/
GTEST_HDRS=third-party/google/gtest/gtest-1.7.0/include
GTEST_LIBS=third-party/google/gtest/
GTEST_MAIN=third-party/google/gtest/gtest-1.7.0/src/gtest_main.cc
SM_SRC=src/securemessage/
SM_TEST=test/securemessage/
CC=g++

TEST_STDERR=test_stderr.log

# TODO: remove the debug and optimization flags when ready for deployment
CFLAGS=-std=gnu++0x -Wall -Wnarrowing -Wextra -Werror -pthread -fPIC -g -O0 -I include -I .

INC_PATH = include/securemessage

PROJ_BASE_OBJECT_FILES = \
  byte_buffer.o \
  crypto_ops.o \
  public_key_proto_util.o \
  securemessage.pb.o \
  raw_secure_message_parser.o \
  secure_message_builder.o \
  secure_message_parser.o \
  secure_message_wrapper.o \
  util.o

PROJ_OPENSSL_OBJECT_FILES = \
  $(PROJ_BASE_OBJECT_FILES) \
  crypto_ops_openssl.o

################################
#
# Note, run
#  make deps
#
# before running anything else

#################################
# Library -- we can build securemessage with a variety of backends
libsecuremessage-openssl.so: ${PROJ_OPENSSL_OBJECT_FILES}
	${CC} -o $@ -shared $^ -lcrypto


#################################
# Running tests
run_tests: run_parser_tests run_proto_util_tests run_openssl_test

run_parser_tests: secure_message_parser_openssl_test
	export LD_LIBRARY_PATH=${PROTOBUF_LIBS}:${LD_LIBRARY_PATH}; ./secure_message_parser_openssl_test 2> ${TEST_STDERR}

run_proto_util_tests: public_key_proto_util_openssl_test
	export LD_LIBRARY_PATH=${PROTOBUF_LIBS}:${LD_LIBRARY_PATH}; ./public_key_proto_util_openssl_test 2> ${TEST_STDERR}

run_openssl_test: crypto_ops_openssl_test
	export LD_LIBRARY_PATH=${PROTOBUF_LIBS}:${LD_LIBRARY_PATH}; ./crypto_ops_openssl_test 2> ${TEST_STDERR}

run_builder_openssl_test: builder_openssl_test
	export LD_LIBRARY_PATH=${PROTOBUF_LIBS}:${LD_LIBRARY_PATH}; ./builder_openssl_test 2> ${TEST_STDERR}


#################################
# Utilities
util.o: ${SM_SRC}/util.cc ${INC_PATH}/util.h
	${CC} ${CFLAGS} -c $<

byte_buffer.o: ${SM_SRC}/byte_buffer.cc ${INC_PATH}/byte_buffer.h
	${CC} ${CFLAGS} -c $<


#################################
# Crypto Operations
crypto_ops.o: ${SM_SRC}/crypto_ops.cc ${INC_PATH}/crypto_ops.h ${INC_PATH}/byte_buffer.h ${INC_PATH}/common.h ${INC_PATH}/secure_message_wrapper.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -c $<

crypto_ops_openssl.o: ${SM_SRC}/crypto_ops_openssl.cc ${INC_PATH}/crypto_ops.h ${INC_PATH}/byte_buffer.h ${INC_PATH}/util.h
	${CC} ${CFLAGS} -c $<

crypto_ops_test.o: ${SM_TEST}/crypto_ops_test.cc crypto_ops.o
	${CC} ${CFLAGS} -I ${GTEST_HDRS} -c $<

crypto_ops_openssl_test: crypto_ops_test.o ${PROJ_OPENSSL_OBJECT_FILES} ${GTEST_MAIN}
	${CC} ${CFLAGS} -I ${GTEST_HDRS} -L ${PROTOBUF_LIBS} -L ${GTEST_LIBS} -o $@ $^ -lgtest -lcrypto -lprotobuf


#################################
# The parser
secure_message_parser.o: ${SM_SRC}/secure_message_parser.cc ${INC_PATH}/secure_message_parser.h securemessage.pb.h ${INC_PATH}/common.h ${INC_PATH}/crypto_ops.h ${INC_PATH}/raw_secure_message_parser.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -c $<

raw_secure_message_parser.o: ${SM_SRC}/raw_secure_message_parser.cc ${INC_PATH}/raw_secure_message_parser.h ${INC_PATH}/crypto_ops.h ${INC_PATH}/common.h ${INC_PATH}/secure_message_wrapper.h
	${CC} ${CFLAGS} -c $<

secure_message_parser_openssl_test: ${SM_TEST}/secure_message_parser_test.cc ${PROJ_OPENSSL_OBJECT_FILES} ${GTEST_MAIN}
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -L ${PROTOBUF_LIBS} -I ${GTEST_HDRS} -L ${GTEST_LIBS} -o $@ $^ -lprotobuf -lgtest -lcrypto


#################################
# The builder
secure_message_builder.o: ${SM_SRC}/secure_message_builder.cc ${INC_PATH}/secure_message_builder.h securemessage.pb.h ${INC_PATH}/byte_buffer.h ${INC_PATH}/common.h ${INC_PATH}/crypto_ops.h ${INC_PATH}/secure_message_wrapper.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -c $<

builder_parser_test.o: ${SM_TEST}/builder_parser_test.cc ${INC_PATH}/secure_message_builder.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -I ${GTEST_HDRS} -c $<

builder_openssl_test: builder_parser_test.o ${PROJ_OPENSSL_OBJECT_FILES} ${GTEST_MAIN}
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -L ${PROTOBUF_LIBS} -I ${GTEST_HDRS} -L ${GTEST_LIBS} -o $@ $^ -lprotobuf -lgtest -lcrypto


#################################
# The protobuf defintion
securemessage.pb.o: securemessage.pb.cc securemessage.pb.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -L ${PROTOBUF_LIBS} -c $< -lprotobuf

securemessage.pb.cc: securemessage.proto
	${PROTOC} --cpp_out=. $<

securemessage.pb.h: securemessage.proto
	${PROTOC} --cpp_out=. $<


##################################
# The Proto Utils
secure_message_wrapper.o: ${SM_SRC}/secure_message_wrapper.cc ${INC_PATH}/secure_message_wrapper.h ${INC_PATH}/crypto_ops.h ${INC_PATH}/common.h securemessage.pb.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -c $<

public_key_proto_util.o: ${SM_SRC}/public_key_proto_util.cc ${INC_PATH}/public_key_proto_util.h ${INC_PATH}/common.h ${INC_PATH}/secure_message_wrapper.h  securemessage.pb.h
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -c $<

public_key_proto_util_openssl_test: ${SM_TEST}/public_key_proto_util_test.cc ${PROJ_OPENSSL_OBJECT_FILES} ${GTEST_MAIN}
	${CC} ${CFLAGS} -I ${PROTOBUF_HDRS} -L ${PROTOBUF_LIBS} -I ${GTEST_HDRS} -L ${GTEST_LIBS} -o $@ $^ -lprotobuf -lgtest -lcrypto


##################################
# Third Party Dependencides
deps: protobuf gtest

protobuf:
	cd ${PROTOBUF_DIR}; ./configure --prefix=`pwd`/../
	make -C ${PROTOBUF_DIR}
	make -C ${PROTOBUF_DIR} check
	make -C ${PROTOBUF_DIR} install

gtest:
	${CC} -isystem ${GTEST_DIR}/include -I ${GTEST_DIR} -pthread -c ${GTEST_DIR}/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o
	mv libgtest.a gtest-all.o ${GTEST_LIBS}


##################################
# Clean up
clean:
	rm -f *.o
	rm -f securemessage.pb.*
	rm -rf *_test
	rm -f core
	rm -f *.so

realclean: clean
	make -C ${PROTOBUF_DIR} clean
	rm -rf ${PROTOBUF_DIR}../{bin,include,lib}
	rm -f ${GTEST_DIR}/../*.{a,o}
