<<<<<<< HEAD
#include <errno.h>
=======
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
<<<<<<< HEAD

#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if CPUINFO_MOCK
#include <cpuinfo-mock.h>
#endif
#include <cpuinfo/log.h>
#include <linux/api.h>
=======
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

#if CPUINFO_MOCK
	#include <cpuinfo-mock.h>
#endif
#include <linux/api.h>
#include <cpuinfo/log.h>

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

/*
 * Size, in chars, of the on-stack buffer used for parsing cpu lists.
 * This is also the limit on the length of a single entry
 * (<cpu-number> or <cpu-number-start>-<cpu-number-end>)
 * in the cpu list.
 */
#define BUFFER_SIZE 256

<<<<<<< HEAD
=======

>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
/* Locale-independent */
inline static bool is_whitespace(char c) {
	switch (c) {
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			return true;
		default:
			return false;
	}
}

inline static const char* parse_number(const char* string, const char* end, uint32_t number_ptr[restrict static 1]) {
	uint32_t number = 0;
	while (string != end) {
<<<<<<< HEAD
		const uint32_t digit = (uint32_t)(*string) - (uint32_t)'0';
=======
		const uint32_t digit = (uint32_t) (*string) - (uint32_t) '0';
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		if (digit >= 10) {
			break;
		}
		number = number * UINT32_C(10) + digit;
		string += 1;
	}
	*number_ptr = number;
	return string;
}

<<<<<<< HEAD
inline static bool parse_entry(
	const char* entry_start,
	const char* entry_end,
	cpuinfo_cpulist_callback callback,
	void* context) {
=======
inline static bool parse_entry(const char* entry_start, const char* entry_end, cpuinfo_cpulist_callback callback, void* context) {
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	/* Skip whitespace at the beginning of an entry */
	for (; entry_start != entry_end; entry_start++) {
		if (!is_whitespace(*entry_start)) {
			break;
		}
	}
	/* Skip whitespace at the end of an entry */
	for (; entry_end != entry_start; entry_end--) {
		if (!is_whitespace(entry_end[-1])) {
			break;
		}
	}

<<<<<<< HEAD
	const size_t entry_length = (size_t)(entry_end - entry_start);
=======
	const size_t entry_length = (size_t) (entry_end - entry_start);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	if (entry_length == 0) {
		cpuinfo_log_warning("unexpected zero-length cpu list entry ignored");
		return false;
	}

<<<<<<< HEAD
#if CPUINFO_LOG_DEBUG_PARSERS
	cpuinfo_log_debug("parse cpu list entry \"%.*s\" (%zu chars)", (int)entry_length, entry_start, entry_length);
#endif
=======
	#if CPUINFO_LOG_DEBUG_PARSERS
		cpuinfo_log_debug("parse cpu list entry \"%.*s\" (%zu chars)", (int) entry_length, entry_start, entry_length);
	#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	uint32_t first_cpu, last_cpu;

	const char* number_end = parse_number(entry_start, entry_end, &first_cpu);
	if (number_end == entry_start) {
		/* Failed to parse the number; ignore the entry */
<<<<<<< HEAD
		cpuinfo_log_warning(
			"invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			entry_start[0],
			(int)entry_length,
			entry_start);
		return false;
	} else if (number_end == entry_end) {
/* Completely parsed the entry */
#if CPUINFO_LOG_DEBUG_PARSERS
		cpuinfo_log_debug(
			"cpulist: call callback with list_start = %" PRIu32 ", list_end = %" PRIu32,
			first_cpu,
			first_cpu + 1);
#endif
=======
		cpuinfo_log_warning("invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			entry_start[0], (int) entry_length, entry_start);
		return false;
	} else if (number_end == entry_end) {
		/* Completely parsed the entry */
		#if CPUINFO_LOG_DEBUG_PARSERS
			cpuinfo_log_debug("cpulist: call callback with list_start = %"PRIu32", list_end = %"PRIu32,
				first_cpu, first_cpu + 1);
		#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		return callback(first_cpu, first_cpu + 1, context);
	}

	/* Parse the second part of the entry */
	if (*number_end != '-') {
<<<<<<< HEAD
		cpuinfo_log_warning(
			"invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			*number_end,
			(int)entry_length,
			entry_start);
=======
		cpuinfo_log_warning("invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			*number_end, (int) entry_length, entry_start);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		return false;
	}

	const char* number_start = number_end + 1;
	number_end = parse_number(number_start, entry_end, &last_cpu);
	if (number_end == number_start) {
		/* Failed to parse the second number; ignore the entry */
<<<<<<< HEAD
		cpuinfo_log_warning(
			"invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			*number_start,
			(int)entry_length,
			entry_start);
=======
		cpuinfo_log_warning("invalid character '%c' in the cpu list entry \"%.*s\": entry is ignored",
			*number_start, (int) entry_length, entry_start);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
		return false;
	}

	if (number_end != entry_end) {
<<<<<<< HEAD
		/* Partially parsed the entry; ignore unparsed characters and
		 * continue with the parsed part */
		cpuinfo_log_warning(
			"ignored invalid characters \"%.*s\" at the end of cpu list entry \"%.*s\"",
			(int)(entry_end - number_end),
			number_start,
			(int)entry_length,
			entry_start);
	}

	if (last_cpu < first_cpu) {
		cpuinfo_log_warning(
			"ignored cpu list entry \"%.*s\": invalid range %" PRIu32 "-%" PRIu32,
			(int)entry_length,
			entry_start,
			first_cpu,
			last_cpu);
		return false;
	}

/* Parsed both parts of the entry; update CPU set */
#if CPUINFO_LOG_DEBUG_PARSERS
	cpuinfo_log_debug(
		"cpulist: call callback with list_start = %" PRIu32 ", list_end = %" PRIu32, first_cpu, last_cpu + 1);
#endif
=======
		/* Partially parsed the entry; ignore unparsed characters and continue with the parsed part */
		cpuinfo_log_warning("ignored invalid characters \"%.*s\" at the end of cpu list entry \"%.*s\"",
			(int) (entry_end - number_end), number_start, (int) entry_length, entry_start);
	}

	if (last_cpu < first_cpu) {
		cpuinfo_log_warning("ignored cpu list entry \"%.*s\": invalid range %"PRIu32"-%"PRIu32,
			(int) entry_length, entry_start, first_cpu, last_cpu);
		return false;
	}

	/* Parsed both parts of the entry; update CPU set */
	#if CPUINFO_LOG_DEBUG_PARSERS
		cpuinfo_log_debug("cpulist: call callback with list_start = %"PRIu32", list_end = %"PRIu32,
			first_cpu, last_cpu + 1);
	#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
	return callback(first_cpu, last_cpu + 1, context);
}

bool cpuinfo_linux_parse_cpulist(const char* filename, cpuinfo_cpulist_callback callback, void* context) {
	bool status = true;
	int file = -1;
	char buffer[BUFFER_SIZE];
<<<<<<< HEAD
#if CPUINFO_LOG_DEBUG_PARSERS
	cpuinfo_log_debug("parsing cpu list from file %s", filename);
#endif
=======
	#if CPUINFO_LOG_DEBUG_PARSERS
		cpuinfo_log_debug("parsing cpu list from file %s", filename);
	#endif
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))

#if CPUINFO_MOCK
	file = cpuinfo_mock_open(filename, O_RDONLY);
#else
	file = open(filename, O_RDONLY);
#endif
	if (file == -1) {
		cpuinfo_log_info("failed to open %s: %s", filename, strerror(errno));
		status = false;
		goto cleanup;
	}

	size_t position = 0;
	const char* buffer_end = &buffer[BUFFER_SIZE];
	char* data_start = buffer;
	ssize_t bytes_read;
	do {
#if CPUINFO_MOCK
<<<<<<< HEAD
		bytes_read = cpuinfo_mock_read(file, data_start, (size_t)(buffer_end - data_start));
#else
		bytes_read = read(file, data_start, (size_t)(buffer_end - data_start));
#endif
		if (bytes_read < 0) {
			cpuinfo_log_info(
				"failed to read file %s at position %zu: %s", filename, position, strerror(errno));
=======
		bytes_read = cpuinfo_mock_read(file, data_start, (size_t) (buffer_end - data_start));
#else
		bytes_read = read(file, data_start, (size_t) (buffer_end - data_start));
#endif
		if (bytes_read < 0) {
			cpuinfo_log_info("failed to read file %s at position %zu: %s", filename, position, strerror(errno));
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			status = false;
			goto cleanup;
		}

<<<<<<< HEAD
		position += (size_t)bytes_read;
		const char* data_end = data_start + (size_t)bytes_read;
		const char* entry_start = buffer;

		if (bytes_read == 0) {
			/* No more data in the file: process the remaining text
			 * in the buffer as a single entry */
=======
		position += (size_t) bytes_read;
		const char* data_end = data_start + (size_t) bytes_read;
		const char* entry_start = buffer;

		if (bytes_read == 0) {
			/* No more data in the file: process the remaining text in the buffer as a single entry */
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			const char* entry_end = data_end;
			const bool entry_status = parse_entry(entry_start, entry_end, callback, context);
			status &= entry_status;
		} else {
			const char* entry_end;
			do {
<<<<<<< HEAD
				/* Find the end of the entry, as indicated by a
				 * comma (',') */
=======
				/* Find the end of the entry, as indicated by a comma (',') */
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
				for (entry_end = entry_start; entry_end != data_end; entry_end++) {
					if (*entry_end == ',') {
						break;
					}
				}

				/*
<<<<<<< HEAD
				 * If we located separator at the end of the
				 * entry, parse it. Otherwise, there may be more
				 * data at the end; read the file once again.
				 */
				if (entry_end != data_end) {
					const bool entry_status =
						parse_entry(entry_start, entry_end, callback, context);
=======
				 * If we located separator at the end of the entry, parse it.
				 * Otherwise, there may be more data at the end; read the file once again.
				 */
				if (entry_end != data_end) {
					const bool entry_status = parse_entry(entry_start, entry_end, callback, context);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
					status &= entry_status;
					entry_start = entry_end + 1;
				}
			} while (entry_end != data_end);

<<<<<<< HEAD
			/* Move remaining partial entry data at the end to the
			 * beginning of the buffer */
			const size_t entry_length = (size_t)(entry_end - entry_start);
=======
			/* Move remaining partial entry data at the end to the beginning of the buffer */
			const size_t entry_length = (size_t) (entry_end - entry_start);
>>>>>>> e3036ca5e16 (Import cpuinfo from commit beb46ca031988 (#6635))
			memmove(buffer, entry_start, entry_length);
			data_start = &buffer[entry_length];
		}
	} while (bytes_read != 0);

cleanup:
	if (file != -1) {
#if CPUINFO_MOCK
		cpuinfo_mock_close(file);
#else
		close(file);
#endif
		file = -1;
	}
	return status;
}
