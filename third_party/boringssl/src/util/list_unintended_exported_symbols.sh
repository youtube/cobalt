#!/bin/sh
#
# Copyright (c) 2025 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Audit source code for identifiers that do not match symbol prefixing
# guidelines.
#
# Note that this tool is a prototype; once prefixing public symbols has been
# implemented, a much simpler tool can be created and integrated directly into
# the build process rather than running as a separate audit with bespoke cache
# files.
#
# TODO(crbug.com/42220000): Eventually port it into the build process.

set -e
set -o pipefail

include_files() {
	for file in "$@"; do
		echo "#include <${file}>"
	done
}

source_to_ast() {
	clang++ \
		-Iinclude \
		-Ithird_party/benchmark/include \
		-Ithird_party/googletest/googlemock/include \
		-Ithird_party/googletest/googletest/include \
		-fsyntax-only \
		-Xclang -ast-dump=json \
		"$@"
}

ast_to_identifiers() {
	go run util/extract_identifiers_clang_json.go "$@"
}

c_source_to_identifiers() {
	source_to_ast -x c -std=c17 "$@" | ast_to_identifiers --language=C
}

cc_source_to_identifiers() {
	source_to_ast -x c++ -std=c++17 "$@" | ast_to_identifiers --language=C++
}

lib_sources() {
	jq  -r '
			[.bcm, .crypto, .decrepit] |
			.[] |
			(.internal_hdrs + .srcs) |
			.[]?
		' gen/sources.json |\
		perl -pe '$_ = "" if /\.inc$/m and not /\.cc\.inc$/m' |\
		grep -vxF 'crypto/curve25519/curve25519_tables.h' |\
		grep -vxF 'crypto/fipsmodule/ec/builtin_curves.h' |\
		grep -vxF 'crypto/fipsmodule/ec/p256-nistz-table.h' |\
		grep -vxF 'crypto/fipsmodule/ec/p256_table.h' |\
		grep -vxF 'crypto/obj/obj_dat.h' |\
		grep -vE '^third_party/.*' |\
		sort -u
}

public_c_includes() {
	jq  -r '
			[.bcm, .crypto, .decrepit] |
			.[] |
			.hdrs |
			.[]?
		' gen/sources.json |\
		sed -e 's,^include/,,'
}

public_cc_includes() {
	# Note: using |hdrs| of _all_ modules. The assumption is that all
	# |hdrs| are public.
	jq  -r '
			.[] |
			.hdrs |
			.[]?
		' gen/sources.json |\
		sed -e 's,^include/,,'
}

set_difference() {
	{
		sort -u < "$1"
		shift
		for f in "$@"; do
			cat "${f}"
			cat "${f}"
		done
	} | sort | uniq -u
}

filter_bssl() {
	# Whatever is in the bssl namespace is OK.
	grep -vE ' bssl::' || true
}

filter_no_symbols() {
	# Whatever is static or an enumerator never becomes part of a symbol.
	grep -vE "^(static|enumerator) " || true
}

filter_expected_symbols() {
	# Ignore types for now (they can become _part_ of a symbol name,
	# but are unlikely to clash).
	grep -vE "^(class|enum|struct|typedef|union|using) " || true
}

filter_stl() {
	# These symbols are often declared to support the STL and are benign,
	# as they have C++ linkage and very specific arguments and thus
	# non-conflicting mangled names.
	grep -vE 'extern "C\+\+" function (begin|end);' || true
}

filter_enum() {
	# Some enums are declared by public headers only in C++.
	# This is intended.
	grep -vE "^enum " || true
}

filter_difference() {
	filter_bssl |\
		filter_no_symbols |\
		filter_expected_symbols |\
		filter_stl
}

fix_c_cc_include_deltas() {
	# OPENSSL_INLINE behaves differently in C and C++.
	sed -e 's,^static ,extern "C" ,g' |\
		filter_bssl |\
		filter_stl |\
		filter_enum |\
		sort -u
}

echo >&2 'Indexing C++ includes...'
include_files $(public_cc_includes) | cc_source_to_identifiers - > include.cc.ids
echo >&2 'Indexing C includes...'
include_files $(public_c_includes) | c_source_to_identifiers - > include.c.ids
echo >&2 'Indexing C includes as C++...'
include_files $(public_c_includes) | cc_source_to_identifiers - > include.c_as_cc.ids

# Check that the headers behave the same if included by C and C++ files, other
# than for expected diffs.
echo >&2 'Comparing C includes across including language...'
fix_c_cc_include_deltas < include.c.ids > include.c.common.ids
fix_c_cc_include_deltas < include.c_as_cc.ids > include.c_as_cc.common.ids
diff -u include.c.common.ids include.c_as_cc.common.ids >&2

# Check that no source file defines any public symbols that are not in the
# public headers, namespaced or otherwise OK'd.
max_jobs=16
set --
wait_jobs() {
	wait
	for f in "$@"; do
		[ -f "${f}" ]
	done
}
for file in $(lib_sources); do
	if ! [ -f "${file}.ids" ]; then
		echo >&2 "Indexing ${file} (new)..."
	elif [ "${file}" -nt "${file}.ids" ]; then
		echo >&2 "Indexing ${file} (changed)..."
		rm -f "${file}.ids"
	else
		perl -nE 'm!// (\S+):! and say $1' "${file}.ids" |\
			sort -u |\
			while read -r dep; do
				if [ "${dep}" -nt "${file}.ids" ]; then
					echo >&2 "Indexing ${file} (dependency ${dep} changed)..."
					rm -f "${file}.ids"
					break
				fi
			done
		if [ -f "${file}.ids" ]; then
			continue
		fi
	fi
	# NOTE: It'd be nice to only recompute ${file}.ids if ${file} _or_ one
	# of its dependencies changed.
	{
		case "$file" in
			*.c)
				c_source_to_identifiers "${file}"
				;;
			**)
				# This includes headers too.
				cc_source_to_identifiers "${file}"
				;;
		esac > "${file}.ids.new"
		mv "${file}.ids.new" "${file}.ids"
	} &
	set -- "$@" "${file}.ids"
	if [ $# -ge ${max_jobs} ]; then
		wait_jobs "$@"
		set --
	fi
done
wait_jobs "$@"

echo >&2 'Comparing symbols...'
for file in $(lib_sources); do
	set_difference "${file}.ids" include.cc.ids | filter_difference
done | sort -u | { ! grep .; }
