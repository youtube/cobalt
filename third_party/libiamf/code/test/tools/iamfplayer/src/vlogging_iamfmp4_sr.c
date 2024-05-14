/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file vlogging_iamfmp4_sr.c
 * @brief verification log generator.
 * @version 0.1
 * @date Created 03/29/2023
 **/

#if defined(__linux__)
#include <unistd.h>
#else
#include <io.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vlogging_iamfmp4_sr.h"

#ifndef _A2B_ENDIAN_H
#define _A2B_ENDIAN_H

#include <stdint.h>

static uint32_t bswap32(const uint32_t u32) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
  return __builtin_bswap32(u32);
#else
  return (u32 << 24) | ((u32 << 8) & 0xFF0000) | ((u32 >> 8) & 0xFF00) |
         (u32 >> 24);
#endif
#else
  return u32;
#endif
}

static uint16_t bswap16(const uint16_t u16) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
  return __builtin_bswap16(u16);
#else
  return (u16 << 8) | (u16 >> 8);
#endif
#else
  return u16;
#endif
}

static uint64_t bswap64(const uint64_t u64) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
  return __builtin_bswap64(u64);
#else
  return ((u64 << 56) & 0xff00000000000000ULL) |
         ((u64 << 40) & 0x00ff000000000000ULL) |
         ((u64 << 24) & 0x0000ff0000000000ULL) |
         ((u64 << 8) & 0x000000ff00000000ULL) |
         ((u64 >> 8) & 0x00000000ff000000ULL) |
         ((u64 >> 24) & 0x0000000000ff0000ULL) |
         ((u64 >> 40) & 0x000000000000ff00ULL) |
         ((u64 >> 56) & 0x00000000000000ffULL);
#endif
#else
  return u64;
#endif
}

#define be64(x) bswap64(x)
#define be32(x) bswap32(x)
#define be16(x) bswap16(x)

#endif

#define LOG_BUFFER_SIZE 10000

////////////////////////////////////////// MP4 atom logging
//////////////////////////////////////

int utc2rstring(uint64_t utc, char* txt, int sizemax) {
  struct tm* new_tm;
  enum { BUFSIZE = 40 };
  char buf[BUFSIZE];
  int j;

  new_tm = gmtime(&utc);
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d UTC",
          new_tm->tm_year + 1834,  // years since 1900
          new_tm->tm_mon + 1,      // months since January - [0, 11]
          new_tm->tm_mday,         // day of the month - [1, 31]
          new_tm->tm_hour,         // hours since midnight - [0, 23]
          new_tm->tm_min,          // minutes after the hour - [0, 59]
          new_tm->tm_sec);  // seconds after the minute - [0, 60] including leap
                            // second
  for (j = 0; j < sizemax; j++) {
    txt[j] = ((char*)buf)[j];
    if (!txt[j]) {
      txt[j] = '\0';
      break;
    }
  }
  if (j < sizemax) {
    txt[j] = '\0';
    return j + 1;
  } else {
    txt[sizemax - 1] = '\0';
    return sizemax;
  }
}

static int queue_rdata(int index, void* data, int size, void* rdata,
                       int sizemax) {
  if (index + size < sizemax) {
    memcpy(rdata, ((char*)data) + index, size);
    return size;
  } else {
    memcpy(rdata, ((char*)data) + index, sizemax);
    return sizemax;
  }
}

static int queue_rstring(int index, void* data, int size, char* txt,
                         int sizemax) {
  int j;
  for (j = 0; j < sizemax && j < size; j++) {
    txt[j] = ((char*)data)[index + j];
    if (!txt[j]) {
      txt[j] = '\0';
      break;
    }
  }

  if (size < sizemax) {
    txt[size] = '\0';
    return size;
  } else {
    txt[sizemax - 1] = '\0';
    return sizemax;
  }
}

static uint64_t queue_rb64(int index, void* data, int size) {
  uint64_t val = 0;
  queue_rdata(index, data, 8, &val, 8);
  val = bswap64(val);
  return val;
}

static uint32_t queue_rb32(int index, void* data, int size) {
  uint32_t val = 0;
  queue_rdata(index, data, 4, &val, 4);
  val = bswap32(val);
  return val;
}

static uint16_t queue_rb16(int index, void* data, int size) {
  uint16_t val = 0;
  queue_rdata(index, data, 2, &val, 2);
  val = bswap16(val);
  return val;
}

static uint8_t queue_rb8(int index, void* data, int size) {
  uint8_t val = 0;
  queue_rdata(index, data, 1, &val, 1);
  return val;
}

static void write_ftyp_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <ftyp>
  <Address Value="0x0000000000000000"/>
  <HeaderSize Value="8"/>
  <DataSize Value="16"/>
  <Size Value="24"/>
  <ParserReadBytes Value="24"/>
  <Version Value="0"/>
  <MajorBrands Value="dash"/>
  <CompatibleBrands Value="iso6 mp41"/>
  </ftyp>
  */
  enum { BUFSIZE = 40 };
  char buf[BUFSIZE];
  uint32_t val;
  int index = 0;

  buf[4] = 0;
  index += queue_rdata(0, atom_d, size, buf, 4);  //  "major brand"
  val = queue_rb32(index, atom_d, size - index);
  index += 4;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "ftyp_%016x:", atom_addr);
  log += write_yaml_form(log, 0, "- MajorBrands: %s", buf);
  log += write_yaml_form(log, 0, "- Version: %u", val);

  index += queue_rstring(index, atom_d, size - index, buf, BUFSIZE);
  log += write_yaml_form(log, 0, "- CompatibleBrands: %s", buf);
  write_postfix(LOG_MP4BOX, log);
}

static void write_mdat_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <mdat>
  <Address Value="0x000000000000052B"/>
  <HeaderSize Value="8"/>
  <DataSize Value="32378"/>
  <Size Value="32386"/>
  <ParserReadBytes Value="32386"/>
  </mdat>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "mdat_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_moov_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <moov>
  <Address Value="0x0000000000000018"/>
  <HeaderSize Value="8"/>
  <DataSize Value="695"/>
  <Size Value="703"/>
  <ParserReadBytes Value="703"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "moov_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_moof_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <moof>
  <Address Value="0x00000000000002D7"/>
  <HeaderSize Value="8"/>
  <DataSize Value="588"/>
  <Size Value="596"/>
  <ParserReadBytes Value="596"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "moof_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_trak_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <Address Value="0x00000000000000B4"/>
  <HeaderSize Value="8"/>
  <DataSize Value="539"/>
  <Size Value="547"/>
  <ParserReadBytes Value="547"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "trak_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_tkhd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <tkhd>
  <Address Value = "0x00000000000000BC" / >
  <HeaderSize Value = "8" / >
  <DataSize Value = "84" / >
  <Size Value = "92" / >
  <ParserReadBytes Value = "92" / >
  <Version Value = "0" / >
  <Flags Value = "0x00000003" / >
  <CreatedDate Value = "2023-03-30 23:56:24 UTC" / >
  <ModifiedDate Value = "2023-03-30 23:56:24 UTC" / >
  <TrackID Value = "1" / >
  <Reserved - 32bits Value = "0x00000000" / >
  <Duration Value = "0" / >
  <Reserved - 32bits Value = "0x00000000" / >
  <Reserved - 32bits Value = "0x00000000" / >
  <Layer Value = "0" / >
  <Group Value = "0" / >
  <Volume Value = "256" / >
  <Reserved - 16bits Value = "0x000 0" / >
  <Matrix Value = "0x00010000 0x00000000 0x00000000 0x00000000 0x00010000
  0x00000000 0x00000000 0x00000000 0x40000000" / > <Width Value = "0" / >
  <Height Value = "0" / >
  </tkhd>
  */
  enum { BUFSIZE = 256, BUFSIZE2 = 40 };
  char buf[BUFSIZE], buf2[BUFSIZE2];
  uint32_t val, x;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "tkhd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // Creation time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf2, BUFSIZE2);
  log += write_yaml_form(log, 0, "- CreationTime: %s", buf2);

  // Modification time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf2, BUFSIZE2);
  log += write_yaml_form(log, 0, "- ModificationTime: %s", buf2);

  // Track ID
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TrackID: %u", val);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved1: %u", val);

  // Duration
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Duration: %u", val);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved2: %u", val);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved3: %u", val);

  // Layer
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Layer: %u", val);

  // Alternativegroup
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- AlternativeGroup: %u", val);

  // Volume
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Volume: %u", val);

  // Reserved
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Reserved4: %u", val);

  // Matrix structure
  buf[0] = 0;
  for (x = 0; x < 9; x++) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    sprintf(buf2, "0x%08x", val);
    strcat(buf, buf2);
    if (x < 8) strcat(buf, " ");
  }
  log += write_yaml_form(log, 0, "- MatrixStructure: %s", buf);

  // Track width
  val = queue_rb16(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TrackWidth: %u", val);

  // Track height
  val = queue_rb16(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TrackHeight: %u", val);

  write_postfix(LOG_MP4BOX, log);
}

static void write_traf_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <traf>
  <Address Value="0x00000000000002EF"/>
  <HeaderSize Value="8"/>
  <DataSize Value="564"/>
  <Size Value="572"/>
  <ParserReadBytes Value="572"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "traf_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_tfhd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <tfhd>
  <Address Value="0x00000000000002F7"/>
  <HeaderSize Value="8"/>
  <DataSize Value="16"/>
  <Size Value="24"/>
  <ParserReadBytes Value="24"/>
  <Version Value="0"/>
  <Flags Value="0x00020022"/>
  <TrackID Value="1"/>
  <SampleDescriptionIndex Value="1"/>
  <DefaultSampleFlags Value="0x00000000"/>
  </tfhd>
  */
  uint32_t val, val2;
  int index = 0;
  int tf_flags = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "tfhd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);
  tf_flags = val & 0xffffff;

  // track_ID
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TrackID: %u", val & 0xffffff);

  if (tf_flags & 0x01) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    val2 = queue_rb32(index, atom_d, size - index);
    index += 4;
    log += write_yaml_form(log, 0, "- BaseDataOffset: %0x08x,%08x", val, val2);
  }

  if (tf_flags & 0x02) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    log += write_yaml_form(log, 0, "- SampleDescriptionIndex: %u", val);
  }

  if (tf_flags & 0x08) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    log += write_yaml_form(log, 0, "- DefaultSampleDuration: %u", val);
  }

  if (tf_flags & 0x10) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    log += write_yaml_form(log, 0, "- DefaultSampleSize: %u", val);
  }

  if (tf_flags & 0x20) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    log += write_yaml_form(log, 0, "- DefaultSampleFlag: %u", val);
  }
  write_postfix(LOG_MP4BOX, log);
}

static void write_trun_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <trun>
  <Address Value="0x000000000000031F"/>
  <HeaderSize Value="8"/>
  <DataSize Value="516"/>
  <Size Value="524"/>
  <ParserReadBytes Value="524"/>
  <Version Value="1"/>
  <Flags Value="0x00000301"/>
  <SampleCount Value="63"/>
  <DataOffset Value="604"/>
  <Entry Index="0">
  <Duration Value="128"/>
  <Size Value="518"/>
  </Entry>
  */
  uint32_t val, vf;
  int index = 0;
  int sample_count = 0;
  int cnt;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "trun_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);
  vf = val & 0xffffff;

  // sample count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- SampleCount: %u", val);
  sample_count = val;

  // data offset
  if (vf & 0x1) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- DataOffset: %u", val);
  }

  // first_sample_flags;
  if (vf & 0x04) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- FirstSampleFlags: %u", val);
  }

  char* tmp = *log;
  *log = (char*)realloc(
      *log, sizeof(char) * (LOG_BUFFER_SIZE - len + 150 * sample_count));
  if (NULL == *log) {
    *log = tmp;
    return;
  }

  // sample entries
  for (cnt = 0; cnt < sample_count; cnt++) {
    if (vf & 0x100) {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      len +=
          write_yaml_form(*log + len, 0, "- SampleDuration_%d: %u", cnt, val);
    }
    if (vf & 0x200) {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      len += write_yaml_form(*log + len, 0, "- SampleSize_%d: %u", cnt, val);
    }
    if (vf & 0x400) {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      len += write_yaml_form(*log + len, 0, "- SampleFlags_%d: %u", cnt, val);
    }
    if (vf & 0x800) {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      len += write_yaml_form(*log + len, 0,
                             "- SampleCompositionTimeOffset_%d: %u", cnt, val);
    }
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

static void write_mvhd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <mvhd>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <CreatedDate Value="2023-03-30 23:56:24 UTC"/>
  <ModifiedDate Value="2023-03-30 23:56:24 UTC"/>
  <Timescale Value="16000"/>
  <Duration Value="0"/>
  <Rate Value="65536"/>
  <Volume Value="256"/>
  <Reserved-16bits Value="0x0000"/>
  <Reserved-32bits Value="0x00000000"/>
  <Reserved-32bits Value="0x00000000"/>
  <Matrix Value="0x00010000 0x00000000 0x00000000 0x00000000 0x00010000
  0x00000000 0x00000000 0x00000000 0x40000000"/> <PreviewTime Value="0"/>
  <PreviewDuration Value="0"/>
  <PosterTime Value="0"/>
  <SelectionTime Value="0"/>
  <SelectionDuration Value="0"/>
  <CurrentTime Value="0"/>
  <NextTrackID Value="2"/>
  </mvhd>
  */
  enum { BUFSIZE = 256, BUFSIZE2 = 40 };
  char buf[BUFSIZE], buf2[BUFSIZE2];
  uint32_t val, x;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "mvhd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // Creation time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf2, BUFSIZE2);
  log += write_yaml_form(log, 0, "- CreationTime: %s", buf2);
  // Modification time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf2, BUFSIZE2);
  log += write_yaml_form(log, 0, "- ModificationTime: %s", buf2);
  // Time scale
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TimeScale: %u", val);
  // Duration
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Duration: %u", val);
  // PreferedRate
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- PreferedRate: %u", val);
  // PreferedVolume
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- PreferedVolume: %u", val);
  // Reserved
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Reserved1: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved2: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved3: %u", val);
  // Matrix structure
  buf[0] = 0;
  for (x = 0; x < 9; x++) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    sprintf(buf2, "0x%08x", val);
    strcat(buf, buf2);
    if (x < 8) strcat(buf, " ");
  }
  log += write_yaml_form(log, 0, "- MatrixStructure: %s", buf);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- PreviewTime: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- PreviewDuration: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- PosterTime: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- SelectionTime: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- SelectionDuration: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- CurrentTime: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- NextTrackID: %u", val);

  write_postfix(LOG_MP4BOX, log);
}

static void write_mvex_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <mvex>
  <Address Value="0x000000000000008C"/>
  <HeaderSize Value="8"/>
  <DataSize Value="32"/>
  <Size Value="40"/>
  <ParserReadBytes Value="40"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "mvex_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_trex_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <trex>
  <Address Value="0x0000000000000094"/>
  <HeaderSize Value="8"/>
  <DataSize Value="24"/>
  <Size Value="32"/>
  <ParserReadBytes Value="32"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <TrackID Value="1"/>
  <DefaultSampleDescriptionIndex Value="1"/>
  <DefaultSampleDuration Value="0"/>
  <DefaultSampleSize Value="0"/>
  <DefaultSampleFlags Value="0x00000000"/>
  </trex>
  */

  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "trex_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // track_ID
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- track_ID: %u", val);
  // default_sample_description_index
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- default_sample_description_index: %u", val);
  // default_sample_duration
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- default_sample_duration: %u", val);
  // default_sample_size
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- default_sample_size: %u", val);
  // default_sample_flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- default_sample_flags: %u", val);

  write_postfix(LOG_MP4BOX, log);
}

static void write_edts_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "edts_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_elst_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  uint32_t val;
  uint64_t val64;
  int index = 0;
  int entry_count;
  uint8_t version;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "elst_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  version = (val >> 24) & 0xff;
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // entry_count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- EntryCount: %u", val);
  entry_count = val;

  for (int i = 0; i < entry_count; ++i) {
    if (version == 1) {
      val64 = queue_rb64(index, atom_d, size - index);
      index += 8;
      log += write_yaml_form(log, 0, "- SegmentDuration_%d: %lld", i, val64);
      val64 = queue_rb64(index, atom_d, size - index);
      index += 8;
      log += write_yaml_form(log, 0, "- MediaTime_%d: %lld", i, val64);
    } else {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      log += write_yaml_form(log, 0, "- SegmentDuration_%d: %u", i, val);
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      log += write_yaml_form(log, 0, "- MediaTime_%d: %u", i, val);
    }
    val = queue_rb16(index, atom_d, size - index);
    index += 2;
    log += write_yaml_form(log, 0, "- MediaRateInteger_%d: %u", i, val);
    val = queue_rb16(index, atom_d, size - index);
    index += 2;
    log += write_yaml_form(log, 0, "- MediaRateFraction_%d: %u", i, val);
  }

  write_postfix(LOG_MP4BOX, log);
}

static void write_mdhd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <mdhd>
  <Address Value="0x0000000000000120"/>
  <HeaderSize Value="8"/>
  <DataSize Value="24"/>
  <Size Value="32"/>
  <ParserReadBytes Value="32"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <CreatedDate Value="2023-03-30 23:56:24 UTC"/>
  <ModifiedDate Value="2023-03-30 23:56:24 UTC"/>
  <Timescale Value="16000"/>
  <Duration Value="0"/>
  <Language Value="LANGUAGE_UNKNOWN"/>
  <Quality Value="0"/>
  </mdhd>
  */
  enum { BUFSIZE = 256 };
  char buf[BUFSIZE];
  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "mdhd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // Creation time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf, BUFSIZE);
  log += write_yaml_form(log, 0, "- CreationTime: %s", buf);
  // Modification time
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  utc2rstring(val, buf, BUFSIZE);
  log += write_yaml_form(log, 0, "- ModificationTime: %s", buf);
  // Time scale
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- TimeScale: %u", val);
  // Duration
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Duration: %u", val);
  // Language
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Language: %u", val);
  // Quality
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Quality: %u", val);

  write_postfix(LOG_MP4BOX, log);
}

static void write_minf_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <Address Value="0x000000000000019F"/>
  <HeaderSize Value="8"/>
  <DataSize Value="304"/>
  <Size Value="312"/>
  <ParserReadBytes Value="312"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "minf_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_hdlr_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <hdlr>
  <Address Value="0x0000000000000140"/>
  <HeaderSize Value="8"/>
  <DataSize Value="87"/>
  <Size Value="95"/>
  <ParserReadBytes Value="95"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <ComponentType Value="...."/>
  <ComponentSubType Value="soun"/>

  <ComponentManufacturer Value="...."/>
  <ComponentFlags Value="0x00000000"/>
  <ComponentFlagsMask Value="0x00000000"/>

  <ComponentName Value="ISO Media file produced by Google Inc. Created on:
  03/30/2023.."/>
  </hdlr>
  */
  enum { BUFSIZE = 256 };
  char buf[BUFSIZE];
  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "hdlr_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // pre_defined
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- PreDefined: %u", val);

  // Component subtype
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- ComponentSubtype: %u", val);

  // reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved1: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved2: %u", val);
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved3: %u", val);

  // Component name
  index += queue_rstring(index, atom_d, size - index, buf, BUFSIZE);
  log += write_yaml_form(log, 0, "- Name: \"%s\"", buf);
  write_postfix(LOG_MP4BOX, log);
}

static void write_smhd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <smhd>
  <Address Value="0x00000000000002C7"/>
  <HeaderSize Value="8"/>
  <DataSize Value="8"/>
  <Size Value="16"/>
  <ParserReadBytes Value="16"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <Balance Value="0"/>
  <Reserved Value="0x0000"/>
  </smhd>
  */
  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "smhd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // Balance
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Balance: %u", val);

  // Reserved
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Reserved: %u", val);

  write_postfix(LOG_MP4BOX, log);
}

static void write_stbl_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <Address Value="0x00000000000001CB"/>
  <HeaderSize Value="8"/>
  <DataSize Value="244"/>
  <Size Value="252"/>
  <ParserReadBytes Value="252"/>
  */
  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "stbl_%016x:", atom_addr);
  write_postfix(LOG_MP4BOX, log);
}

static void write_stsd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <stsd>
  <Address Value="0x00000000000001D3"/>
  <HeaderSize Value="8"/>
  <DataSize Value="168"/>
  <Size Value="176"/>
  <ParserReadBytes Value="176"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <DescriptorCount Value="1"/>
  */
  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "stsd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Version: %u", (val >> 24) & 0xff);
  log += write_yaml_form(log, 0, "- Flags: %u", val & 0xffffff);

  // Number of entries
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- EntryCount: %u", val);
  write_postfix(LOG_MP4BOX, log);
}

/*
class obu_header() {
        unsigned int(5) obu_type;
        unsigned int(1) obu_redundant_copy;
        unsigned int(1) obu_trimming_status_flag;
        unsigned int(1) obu_extension_flag;
        leb128() obu_size;

        if (obu_trimming_status_flag) {
                leb128() num_samples_to_trim_at_end;
                leb128() num_samples_to_trim_at_start;
        }
        if (obu_extension_flag == 1)
                leb128() extension_header_size;
}
*/

typedef struct _IAMF_OBU {
  uint8_t* data;
  uint64_t size;

  uint8_t type;
  uint8_t redundant;
  uint8_t trimming;
  uint8_t extension;

  uint64_t trim_start;
  uint64_t trim_end;

  uint8_t* ext_header;
  uint64_t ext_size;

  uint8_t* payload;
} IAMF_OBU_t;

uint64_t read_leb128(const uint8_t* data, uint64_t* pret) {
  uint64_t ret = 0;
  uint32_t i;
  uint8_t byte;

  for (i = 0; i < 8; i++) {
    byte = data[i];
    ret |= (((uint64_t)byte & 0x7f) << (i * 7));
    if (!(byte & 0x80)) {
      break;
    }
  }
  ++i;
  *pret = ret;
  return i;
}

#define IAMF_OBU_MIN_SIZE 2
static uint32_t read_IAMF_OBU(const uint8_t* data, uint32_t size,
                              IAMF_OBU_t* obu) {
  uint64_t obu_end = 0;
  uint64_t index = 0;
  uint8_t val;

  if (size < IAMF_OBU_MIN_SIZE) {
    return 0;
  }

  memset(obu, 0, sizeof(IAMF_OBU_t));
  val = data[index];

  obu->type = (val & 0xF8) >> 3;
  obu->redundant = (val & 0x04) >> 2;
  obu->trimming = (val & 0x02) >> 1;
  obu->extension = (val & 0x01);
  index++;

  index += (int)read_leb128(data + index, &obu->size);
  obu_end = index + obu->size;

  if (index + obu->size > size) {
    return 0;
  }
  obu->data = (uint8_t*)data + index;

  if (obu->trimming) {
    index += read_leb128(obu->data + index,
                         &obu->trim_end);  // num_samples_to_trim_at_end;
    index += read_leb128(obu->data + index,
                         &obu->trim_start);  // num_samples_to_trim_at_start;
  }

  if (obu->extension) {
    index += read_leb128(obu->data + index,
                         &obu->ext_size);  // extension_header_size;
    obu->ext_header = (uint8_t*)obu->data + index;
    index += obu->ext_size;
  }

  obu->payload = (uint8_t*)data + index;
  return (uint32_t)obu_end;
}

#define OBU_IA_Codec_Config 0
#define CODEC_ID(c1, c2, c3, c4) \
  (((c1) << 24) + ((c2) << 16) + ((c3) << 8) + ((c4) << 0))

static void write_iamd_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  write_postfix(LOG_MP4BOX, log);
}

static void write_iamf_atom_log(char* log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <Address Value = "0x00000000000001E3" / >
  <HeaderSize Value = "8" / >
  <DataSize Value = "152" / >
  <Size Value = "160" / >
  <ParserReadBytes Value = "160" / >
  <Reserved - 32bits Value = "0x00000000" / >
  <Reserved - 16bits Value = "0x0000" / >
  <DataReferenceIndex Value = "1" / >
  <Version Value = "0" / >
  <Revision Value = "0" / >
  <Vendor Value = "...." / >
  <NumChannels Value = "2" / >
  <SampleBits Value = "16" / >
  <CompressionID Value = "0" / >
  <PacketSize Value = "0" / >
  <SampleRate Value = "16000" / >
  configOBUs
  */
  uint32_t val;
  int index = 0;

  log += write_prefix(LOG_MP4BOX, log);
  log += write_yaml_form(log, 0, "iamf_%016x:", atom_addr);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved1: %u", val);

  // Reserved
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Reserved2: %u", val);

  // Data Reference Index
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- DataReferenceIndex: %u", val);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved3: %u", val);

  // Reserved
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- Reserved4: %u", val);

  // Channel Count
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- ChannelCount: %u", val);

  // Sample Size
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- SampleSize: %u", val);

  // Pre-defined
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Predefined: %u", val);

  // Reserved
  val = queue_rb16(index, atom_d, size - index);
  index += 2;
  log += write_yaml_form(log, 0, "- Reserved5: %u", val);

  // Sample Rate
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  log += write_yaml_form(log, 0, "- SampleRate: %u", val >> 16);

  // configOBUs
  int ret = 0;
  uint64_t x = 0;
  IAMF_OBU_t obu;
  uint64_t val64;

  // log += write_prefix(LOG_MP4BOX, log);
  // log += write_yaml_form(log, 0, "iamd_%016x:", atom_addr);

  /*
  [ ] ia_sequence_header_obu() is the ia_sequence_header_obu() in this set of
  descriptor OBUs. [o] codec_config_obu() is the codec_config_obu() in this set
  of descriptor OBUs. [ ] audio_element_obu() is the i-th audio_element_obu() in
  this set of descriptor OBUs. [ ] mix_presentation_obu() is the j-th
  mix_presentation_obu() in this set of descriptor OBUs. : print only [o]
  codec_config_obu()
  */

  while (index < size) {
    ret = read_IAMF_OBU((const uint8_t*)atom_d + index, size - index, &obu);
    if (ret == 0) break;
    index += ret;
    if (obu.type == OBU_IA_Codec_Config) {
      /*
  class codec_config_obu() {
     leb128() codec_config_id;
     codec_config();
  }

  class codec_config() {
     unsigned int(32) codec_id;
     leb128() num_samples_per_frame;
     signed int(16) audio_roll_distance;
     decoder_config(codec_id);
  }
  */
      x = 0;
      x += read_leb128(obu.payload + x, &val64);
      log += write_yaml_form(log, 0, "- codec_config_id: %u", (int)val64);

      val = queue_rb32((int)x, obu.payload, (int)(obu.size - x));
      x += 4;

      switch (val) {
        case CODEC_ID('O', 'p', 'u', 's'):
          log += write_yaml_form(log, 0, "- codec_id: Opus");
          break;
        case CODEC_ID('m', 'p', '4', 'a'):
          log += write_yaml_form(log, 0, "- codec_id: mp4a");
          break;
        case CODEC_ID('f', 'L', 'a', 'C'):
          log += write_yaml_form(log, 0, "- codec_id: fLaC");
          break;
        case CODEC_ID('i', 'p', 'c', 'm'):
          log += write_yaml_form(log, 0, "- codec_id: ipcm");
          break;
      }

      x += read_leb128(obu.payload + x, &val64);
      log += write_yaml_form(log, 0, "- num_samples_per_frame: %u", (int)val64);

      val = queue_rb16((int)x, obu.payload, (int)(obu.size - x));
      x += 2;
      log += write_yaml_form(log, 0, "- audio_roll_distance: %d", (int16_t)val);
    }
  }
  write_postfix(LOG_MP4BOX, log);
}

static void write_stts_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <stts>
  <Address Value="0x0000000000000283"/>
  <HeaderSize Value="8"/>
  <DataSize Value="8"/>
  <Size Value="16"/>
  <ParserReadBytes Value="16"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <EntryCount Value="0"/>
  </stts>
  */
  uint32_t val;
  int index = 0;
  int entry_count = 0;
  int cnt;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "stts_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);

  // entry count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- EntryCount: %u", val);

  char* tmp = *log;
  *log =
      (char*)realloc(*log, sizeof(char) * (LOG_BUFFER_SIZE - len + 100 * val));
  if (NULL == *log) {
    *log = tmp;
    return;
  }

  entry_count = val;
  for (cnt = 0; cnt < entry_count; cnt++) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- SampleCount_%d: %u", cnt, val);
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- SampleDelta_%d: %u", cnt, val);
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

static void write_stsc_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <stsc>
  <Address Value="0x0000000000000293"/>
  <HeaderSize Value="8"/>
  <DataSize Value="8"/>
  <Size Value="16"/>
  <ParserReadBytes Value="16"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <EntryCount Value="0"/>
  </stsc>
  */
  uint32_t val;
  int index = 0;
  int entry_count = 0;
  int cnt;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "stsc_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);

  // entry count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- EntryCount: %u", val);

  char* tmp = *log;
  *log =
      (char*)realloc(*log, sizeof(char) * (LOG_BUFFER_SIZE - len + 100 * val));
  if (NULL == *log) {
    *log = tmp;
    return;
  }

  entry_count = val;
  for (cnt = 0; cnt < entry_count; cnt++) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- FirstChunk_%d: %u", cnt, val);
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- SamplePerChunk_%d: %u", cnt, val);
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- SampleDescriptionIndex_%d: %u",
                           cnt, val);
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

static void write_stsz_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <stsz>
  <Address Value="0x00000000000002B3"/>
  <HeaderSize Value="8"/>
  <DataSize Value="12"/>
  <Size Value="20"/>
  <ParserReadBytes Value="20"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <DefaultSampleSize Value="0"/>
  <SampleCount Value="0"/>
  </stsz>
  */
  uint32_t val;
  int index = 0;
  int sample_count = 0;
  int sample_size = 0;
  int cnt;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "stsz_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);

  // sample size
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- SampleSize: %u", val);
  sample_size = val;

  // sample count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- SampleCount: %u", val);
  sample_count = val;

  if (sample_size == 0) {
    char* tmp = *log;
    *log = (char*)realloc(
        *log, sizeof(char) * (LOG_BUFFER_SIZE - len + 50 * sample_count));
    if (NULL == *log) {
      *log = tmp;
      return;
    }

    for (cnt = 0; cnt < sample_count; cnt++) {
      val = queue_rb32(index, atom_d, size - index);
      index += 4;
      len += write_yaml_form(*log + len, 0, "- EntrySize_%d: %u", cnt, val);
    }
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

static void write_stco_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  <stco>
  <Address Value="0x00000000000002A3"/>
  <HeaderSize Value="8"/>
  <DataSize Value="8"/>
  <Size Value="16"/>
  <ParserReadBytes Value="16"/>
  <Version Value="0"/>
  <Flags Value="0x00000000"/>
  <EntryCount Value="0"/>
  </stco>
  */
  uint32_t val;
  int index = 0;
  int entry_count = 0;
  int cnt;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "stco_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);

  // entry count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  len += write_yaml_form(*log + len, 0, "- EntryCount: %u", val);

  char* tmp = *log;
  *log =
      (char*)realloc(*log, sizeof(char) * (LOG_BUFFER_SIZE - len + 50 * val));
  if (NULL == *log) {
    *log = tmp;
    return;
  }

  entry_count = val;
  for (cnt = 0; cnt < entry_count; cnt++) {
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    len += write_yaml_form(*log + len, 0, "- ChunkOffset_%d: %u", cnt, val);
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

static void write_sgpd_atom_log(char** log, void* atom_d, int size,
                                uint64_t atom_addr) {
  /*
  [sgpd: Sample Group Description Box]
  position = 727
  size = 26
  version = 1
  flags = 0x000000
  grouping_type = roll
  default_length = 2 (constant)
  entry_count = 1
  roll_distance[0] = -2

  aligned(8) class SampleGroupDescriptionBox (unsigned int(32) handler_type)
  extends FullBox('sgpd', version, 0){
  unsigned int(32) grouping_type;
  if (version>=1) { unsigned int(32) default_length; }
  if (version>=2) {
  unsigned int(32) default_group_description_index;
  }
  unsigned int(32) entry_count;
  int i;
  for (i = 1 ; i <= entry_count ; i++){
          if (version>=1) {
                  if (default_length==0) {
                          unsigned int(32) description_length;
                  }
          }
          SampleGroupDescriptionEntry (grouping_type);
          // an instance of a class derived from SampleGroupDescriptionEntry
          //  that is appropriate and permitted for the media type
  }
  }

  class AudioRollRecoveryEntry() extends AudioSampleGroupEntry ('roll')
  {
  signed int(16) roll_distance;
  }
  */
  enum { BUFSIZE = 256, BUFSIZE2 = 40 };
  char buf[BUFSIZE], buf2[BUFSIZE2], buf3[BUFSIZE * 5 + 1];
  uint32_t val;
  int index = 0;
  int version, grouping_type;
  int default_length, default_group_description_index;
  int entry_count = 0;
  int cnt, x;
  int description_length, remaining, data_size;

  int len = write_prefix(LOG_MP4BOX, *log);
  len += write_yaml_form(*log + len, 0, "sgpd_%016x:", atom_addr);

  // version/flags
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  version = (val >> 24) & 0xff;
  len += write_yaml_form(*log + len, 0, "- Version: %u", (val >> 24) & 0xff);
  len += write_yaml_form(*log + len, 0, "- Flags: %u", val & 0xffffff);

  // grouping_type
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  grouping_type = val;
  len += write_yaml_form(*log + len, 0, "- GroupingType: %u", val);

  if (version >= 1) {  // default_length
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    default_length = val;
    len += write_yaml_form(*log + len, 0, "- DefaultLength: %u", val);
  }
  if (version >= 2) {  // default_group_description_index
    val = queue_rb32(index, atom_d, size - index);
    index += 4;
    default_group_description_index = val;
    len += write_yaml_form(*log + len, 0, "- DefaultGroupDescriptionIndex: %u",
                           val);
  }

  // entry_count
  val = queue_rb32(index, atom_d, size - index);
  index += 4;
  entry_count = val;
  len += write_yaml_form(*log + len, 0, "- EntryCount: %u", val);

  for (cnt = 0; cnt < entry_count; cnt++) {
    if (version >= 1) {
      if (default_length == 0) {
        val = queue_rb32(index, atom_d, size - index);
        index += 4;
        description_length = val;
        len += write_yaml_form(*log + len, 0, "- DescriptionLength_%d: %u", cnt,
                               val);
      } else {
        if (default_length == 1) {
          val = queue_rb8(index, atom_d, size - index);
          index += 1;
          len += write_yaml_form(*log + len, 0, "- GroupingEntryVal_%d: %d",
                                 cnt, (int8_t)val);
        } else if (default_length == 2) {
          val = queue_rb16(index, atom_d, size - index);
          index += 2;
          len += write_yaml_form(*log + len, 0, "- GroupingEntryVal_%d: %d",
                                 cnt, (int16_t)val);
        } else if (default_length == 4) {
          val = queue_rb32(index, atom_d, size - index);
          index += 4;
          len += write_yaml_form(*log + len, 0, "- GroupingEntryVal_%d: %d",
                                 cnt, (int32_t)val);
        } else {
          remaining = default_length;
          buf3[0] = 0;
          while (remaining > 0) {
            if (remaining > BUFSIZE) {
              index += queue_rdata(0, atom_d, size, buf, BUFSIZE);
              data_size = BUFSIZE;
            } else {
              data_size = remaining;
            }

            for (x = 0; x < data_size; x++) {
              val = (uint32_t)(((uint8_t*)buf)[x]);
              sprintf(buf2, "0x%02x ", val);
              strcat(buf3, buf2);
            }
            len += write_yaml_form(*log + len, 0, "- GroupingEntryVal_%d: %s",
                                   cnt, buf3);
          }
        }
      }
    }
  }

  write_postfix(LOG_MP4BOX, *log + len);
}

int vlog_atom(uint32_t atom_type, void* atom_d, int size, uint64_t atom_addr) {
  if (!is_vlog_file_open()) return -1;

  uint64_t key;

  char* log = (char*)calloc(LOG_BUFFER_SIZE, sizeof(char));
  if (NULL == log) return -1;

  key = atom_addr;

  switch (atom_type) {
    case MP4BOX_FTYP:
      write_ftyp_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MOOV:
      write_moov_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MVHD:
      write_mvhd_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MVEX:
      write_mvex_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TREX:
      write_trex_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_EDTS:
      write_edts_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_ELST:
      write_elst_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MDHD:
      write_mdhd_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_HDLR:
      write_hdlr_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MINF:
      write_minf_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STBL:
      write_stbl_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STSD:
      write_stsd_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STTS:
      write_stts_atom_log(&log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STSC:
      write_stsc_atom_log(&log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STSZ:
      write_stsz_atom_log(&log, atom_d, size, atom_addr);
      break;
    case MP4BOX_STCO:
      write_stco_atom_log(&log, atom_d, size, atom_addr);
      break;
    case MP4BOX_SGPD:
      write_sgpd_atom_log(&log, atom_d, size, atom_addr);
      break;
    case MP4BOX_MOOF:
      write_moof_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TRAK:
      write_trak_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TKHD:
      write_tkhd_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TFHD:
      write_tfhd_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TRAF:
      write_traf_atom_log(log, atom_d, size, atom_addr);
      break;
    case MP4BOX_TRUN:
      write_trun_atom_log(&log, atom_d, size, atom_addr);
      break;

    /* Immersive audio atom */
    case MP4BOX_IAMF:
      write_iamf_atom_log(log, atom_d, size, atom_addr);
      break;

    case MP4BOX_MDAT:
      write_mdat_atom_log(log, atom_d, size, atom_addr);
      break;
  };

  int ret = vlog_print(LOG_MP4BOX, key, log);
  if (log) free(log);

  return ret;
}
