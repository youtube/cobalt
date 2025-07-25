/* Copyright 2019 Google Inc. All Rights Reserved.
** Licensed under the Apache License, Version 2.0 (the "License");
**
** Derived from the public domain SQLite (https://sqlite.org) sources.
*/

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

#include "third_party/sqlite/sqlite3.h"

sqlite3 *g_database = 0; /* The database connection */

enum class ColumnType {
  kMin = 0,
  kShortNumber = 0,
  kBlob = 1,
  kString = 2,
  kAny = 3,
  kMax = 3,
};

enum class TableOperator : unsigned long {
  kMin = 0,
  kShadowTableMin = 0,
  kSegdir = 0,
  kContent = 1,
  kDocsize = 2,
  kSegments = 3,
  kStat = 4,
  kShadowTableMax = 4,
  // 5-7: reserved for further use
  kFuzzTable = 8,
  kMax = 8,
};

typedef struct {
  uint8_t op_type : 4;
  uint8_t column_op : 4;
  uint8_t op_sql_operation : 4;
  uint8_t select_operator_1 : 1;
  uint8_t select_operator_2 : 3;
} opdata_16;

/*
** Callback for sqlite3_exec().
*/
static int ExecHandler(void *pCnt, int argc, char **argv, char **namev) {
  return ((*static_cast<int *>(pCnt))--) <= 0;
}

size_t GetOperator16(FuzzedDataProvider *data_provider, opdata_16 *op16) {
  if (data_provider->remaining_bytes() < sizeof(uint16_t))
    return 0;

  uint16_t operator_data = data_provider->ConsumeIntegral<uint16_t>();
  memcpy(op16, &operator_data, sizeof(uint16_t));
  return sizeof(uint16_t);
}

std::string GetValueByType(FuzzedDataProvider *data_provider, ColumnType type) {
  std::string out;
  std::ostringstream ss;
  switch (type) {
  case ColumnType::kShortNumber: {
    uint8_t number = data_provider->ConsumeIntegral<uint8_t>();
    ss << static_cast<uint16_t>(number);
  } break;
  case ColumnType::kBlob: {
    uint16_t length = data_provider->ConsumeIntegral<uint16_t>();
    uint16_t value = 0;
    if (length) {
      ss << "x'" << std::hex;
      for (uint16_t i = 0; i < length; i++) {
        value = data_provider->ConsumeIntegral<uint8_t>();

        if (data_provider->remaining_bytes() == 0)
          break;

        ss << std::setfill('0') << std::setw(2) << value;
      }
      ss << "'";
    } else
      return "x'00'";
  } break;
  default:
    return "'NOT SUPPORTED'";
  }
  out = ss.str();
  return out;
}

void RunSqlQuery(std::string &query, int *exec_count) {
  static bool should_print = ::getenv("DUMP_NATIVE_INPUT");
  if (should_print)
    std::cout << query << std::endl;
  char *zErrMsg = 0; /* Error message returned by sqlite_exec() */
  sqlite3_exec(g_database, query.c_str(), ExecHandler,
               static_cast<void *>(exec_count), &zErrMsg);
  sqlite3_free(zErrMsg);
}

int InitializeDB(FuzzedDataProvider *data_provider, int *exec_count) {
  int rc; /* Return code from various interfaces */
  const char *icu_list[219] = {
      "af_NA",      "af_ZA",       "ar_AE",      "ar_BH",      "ar_DZ",
      "ar_EG",      "ar_IQ",       "ar_JO",      "ar_KW",      "ar_LB",
      "ar_LY",      "ar_MA",       "ar_OM",      "ar_QA",      "ar_SA",
      "ar_SD",      "ar_SY",       "ar_TN",      "ar_YE",      "as_IN",
      "az_Latn",    "az_Latn_AZ",  "be_BY",      "bg_BG",      "bn_BD",
      "bn_IN",      "bs_BA",       "ca_ES",      "chr",        "chr_US",
      "cs_CZ",      "cy_GB",       "da_DK",      "de_AT",      "de_BE",
      "de_CH",      "de_DE",       "de_LI",      "de_LU",      "el_CY",
      "el_GR",      "en",          "en_AS",      "en_AU",      "en_BE",
      "en_BW",      "en_BZ",       "en_CA",      "en_GB",      "en_GU",
      "en_HK",      "en_IE",       "en_IN",      "en_JM",      "en_MH",
      "en_MP",      "en_MT",       "en_MU",      "en_NA",      "en_NZ",
      "en_PH",      "en_PK",       "en_SG",      "en_TT",      "en_UM",
      "en_US",      "en_US_POSIX", "en_VI",      "en_ZA",      "en_ZW",
      "es_419",     "es_AR",       "es_BO",      "es_CL",      "es_CO",
      "es_CR",      "es_DO",       "es_EC",      "es_ES",      "es_GQ",
      "es_GT",      "es_HN",       "es_MX",      "es_NI",      "es_PA",
      "es_PE",      "es_PR",       "es_PY",      "es_SV",      "es_US",
      "es_UY",      "es_VE",       "et_EE",      "fa_IR",      "fi_FI",
      "fil_PH",     "fo_FO",       "fr_BE",      "fr_BF",      "fr_BI",
      "fr_BJ",      "fr_BL",       "fr_CA",      "fr_CD",      "fr_CF",
      "fr_CG",      "fr_CH",       "fr_CI",      "fr_CM",      "fr_DJ",
      "fr_FR",      "fr_GA",       "fr_GN",      "fr_GP",      "fr_GQ",
      "fr_KM",      "fr_LU",       "fr_MC",      "fr_MF",      "fr_MG",
      "fr_ML",      "fr_MQ",       "fr_NE",      "fr_RE",      "fr_RW",
      "fr_SN",      "fr_TD",       "fr_TG",      "ga",         "ga_IE",
      "gu_IN",      "ha_Latn",     "ha_Latn_GH", "ha_Latn_NE", "ha_Latn_NG",
      "he_IL",      "hi_IN",       "hr_HR",      "hu_HU",      "hy_AM",
      "id",         "id_ID",       "ig_NG",      "is_IS",      "it",
      "it_CH",      "it_IT",       "ja_JP",      "ka",         "ka_GE",
      "kk_KZ",      "kl_GL",       "kn_IN",      "ko_KR",      "kok_IN",
      "lt_LT",      "lv_LV",       "mk_MK",      "ml_IN",      "mr_IN",
      "ms",         "ms_BN",       "ms_MY",      "mt_MT",      "nb_NO",
      "nlnl_BE",    "nl_NL",       "nn_NO",      "om_ET",      "om_KE",
      "or_IN",      "pa_Arab",     "pa_Arab_PK", "pa_Guru",    "pa_Guru_IN",
      "pl_PL",      "ps_AF",       "pt",         "pt_BR",      "pt_PT",
      "ro_MD",      "ro_RO",       "ru_MD",      "ru_RU",      "ru_UA",
      "si_LK",      "sk_SK",       "sl_SI",      "sq_AL",      "sr_Cyrl",
      "sr_Cyrl_BA", "sr_Cyrl_ME",  "sr_Cyrl_RS", "sr_Latn_BA", "sr_Latn_ME",
      "sr_Latn_RS", "sv_FI",       "sv_SE",      "sw",         "sw_KE",
      "sw_TZ",      "ta_IN",       "ta_LK",      "te_IN",      "th_TH",
      "tr_TR",      "uk_UA",       "ur_IN",      "ur_PK",      "vi_VN",
      "yo_NG",      "zh_Hans",     "zh_Hans_CN", "zh_Hans_SG", "zh_Hant_HK",
      "zh_Hant_MO", "zh_Hant_TW",  "zu",         "zu_ZA"};

  rc = sqlite3_initialize();
  if (rc)
    return rc;

  /* Open the database connection.  Only use an in-memory database. */
  rc = sqlite3_open_v2(
      "fuzz.db", &g_database,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, 0);
  if (rc)
    return rc;

  /* enables foreign key constraints */
  sqlite3_db_config(g_database, SQLITE_DBCONFIG_ENABLE_FKEY, 1, &rc);

  /* disable defence-in-depth to simplify the fuzzing on shadow tables */
  sqlite3_db_config(g_database, SQLITE_DBCONFIG_DEFENSIVE, 0, &rc);
  if (rc)
    return rc;

  /* determine a limit on the number of output rows */
  *exec_count = 0x3f;

  /* Some initial queries */
  std::string init_drop_db("DROP TABLE IF EXISTS f;");
  std::string init_drop_docsize("DROP TABLE IF EXISTS 'f_docsize';");
  std::string init_drop_stat("DROP TABLE IF EXISTS 'f_stat';");
  std::string init_create_fts3 = "CREATE VIRTUAL TABLE f USING fts3(a,b";
  switch (data_provider->ConsumeIntegralInRange<uint8_t>(0, 3)) {
  case 1:
    init_create_fts3 += ",tokenize=porter";
    break;
  case 2:
    init_create_fts3 += ",tokenize=icu ";
    init_create_fts3 +=
        icu_list[(data_provider->ConsumeIntegralInRange<uint8_t>(
            0, ((sizeof(icu_list) / sizeof(const char *) % 0x100) - 1)))];
    break;
  case 3:
    init_create_fts3 += ",tokenize=icu";
    break;
  default:
  case 0:
    /*if we don't set tokenizer to anything, it will goes default simple
     * tokenizer*/
    break;
  }
  init_create_fts3 += ");";
  std::string create_fake_docsize(
      "CREATE TABLE 'f_docsize'(docid INTEGER PRIMARY KEY, size BLOB);");
  std::string create_fake_stat(
      "CREATE TABLE 'f_stat'(id INTEGER PRIMARY KEY, value BLOB);");
  std::string init_set_initial_data("INSERT INTO f VALUES (1, '1234');");

  RunSqlQuery(init_drop_db, exec_count);
  RunSqlQuery(init_drop_docsize, exec_count);
  RunSqlQuery(init_drop_stat, exec_count);
  RunSqlQuery(init_create_fts3, exec_count);
  RunSqlQuery(create_fake_docsize, exec_count);
  RunSqlQuery(create_fake_stat, exec_count);
  RunSqlQuery(init_set_initial_data, exec_count);

  return rc;
}
/*
** Main entry point.  The fuzzer invokes this function with each
** fuzzed input.
*/

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 3 || size > 0x1000000)
    return 0; /* Early out if unsufficient or too much data */

  FuzzedDataProvider data_provider(data, size);
  int exec_count = 0; /* Abort row callback when count reaches zero */

  if (InitializeDB(&data_provider, &exec_count))
    return 0;

  while (1) {
    std::string op, target, target_column;
    ColumnType target_column_type = ColumnType::kMin;

    opdata_16 op_16;
    size_t ret = GetOperator16(&data_provider, &op_16);

    if (ret == 0)
      break;
    unsigned long table_operator_border =
        static_cast<unsigned long>(TableOperator::kMax) + 1;
    TableOperator op_type =
        static_cast<TableOperator>(op_16.op_type % table_operator_border);

    /*
        1. choose a table and a column to fuzz.
    */
    bool op_on_shadow = (op_type <= TableOperator::kShadowTableMax);
    if (op_on_shadow) {
      switch (op_type) {
      case TableOperator::kSegdir:
        target = "f_segdir";
        switch (op_16.column_op % 5) {
        case 0x00:
          target_column = " level ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x01:
          target_column = " start_block ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x02:
          target_column = " end_block ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x03:
          target_column = " leaves_end_block ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x04:
          target_column = " root ";
          target_column_type = ColumnType::kBlob;
          break;
        }
        break;

      case TableOperator::kContent:
        target = "f_content";
        switch (op_16.column_op % 3) {
        case 0x00:
          target_column = " docid ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x01:
          target_column = " 'c0a' ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x02:
          target_column = " 'c1b' ";
          target_column_type = ColumnType::kBlob;
          break;
        }
        break;

      case TableOperator::kDocsize:
        target = "f_docsize";
        switch (op_16.column_op % 2) {
        case 0x00:
          target_column = " docid ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x01:
          target_column = " size ";
          target_column_type = ColumnType::kBlob;
          break;
        }
        break;

      case TableOperator::kSegments:
        target = "f_segments";
        switch (op_16.column_op % 2) {
        case 0x00:
          target_column = " blockid ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x01:
          target_column = " block ";
          target_column_type = ColumnType::kBlob;
          break;
        }
        break;

      case TableOperator::kStat:
      default:
        target = "f_stat";
        switch (op_16.column_op % 2) {
        case 0x00:
          target_column = " id ";
          target_column_type = ColumnType::kShortNumber;
          break;
        case 0x01:
          target_column = " value ";
          target_column_type = ColumnType::kBlob;
          break;
        }
        break;
      }
    } else {
      target = "f";
      switch (op_16.column_op % 2) {
      case 0x00:
        target_column = " a ";
        target_column_type = ColumnType::kShortNumber;
        break;
      case 0x01:
        target_column = " b ";
        target_column_type = ColumnType::kBlob;
        break;
      }
      op_type = TableOperator::kFuzzTable;
    }

    /*
        2. choose a verb and generate some data if needed
    */
    switch (op_16.op_sql_operation % 6) {
    case 0x01:
      op = "UPDATE " + target + " SET " + target_column + " = ";
      op += GetValueByType(&data_provider, target_column_type);
      op += " WHERE " + target_column + " IN (SELECT " + target_column;
      op += " FROM " + target + " LIMIT 1 OFFSET ";
      op += GetValueByType(&data_provider, ColumnType::kShortNumber);
      op += ");";
      break;

    case 0x00:
    case 0x02: {
      std::ostringstream ss;
      switch (op_type) {
      case TableOperator::kSegdir:
        ss << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << ",'"
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << " "
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << "',"
           << GetValueByType(&data_provider, ColumnType::kBlob);
        break;
      case TableOperator::kContent:
        ss << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kBlob);
        break;
      default:
        /*
            All other tables have the same type, so to simplify, use this
           default instead.
            BLOB is almost the same as STRING and it avoids the annoying
           encoding problem, so choose BLOB instead when some columns
           need STRING
        */
        ss << GetValueByType(&data_provider, ColumnType::kShortNumber) << ","
           << GetValueByType(&data_provider, ColumnType::kBlob);
        break;
      }
      op = "INSERT INTO " + target + " VALUES (" + ss.str() + ");";
    } break;

    case 0x03:
      op = "DELETE FROM f WHERE ";
      op += (op_16.select_operator_1 ? "a" : "b");
      op += "=";
      op += GetValueByType(&data_provider, op_16.select_operator_1
                                               ? ColumnType::kShortNumber
                                               : ColumnType::kBlob);
      op += ";";
      break;

    case 0x04: {
      uint8_t selector_indicator =
          data_provider.ConsumeIntegralInRange<uint8_t>(0, 4);
      std::string selector_string;
      switch (selector_indicator) {
      case 0x00: {
        selector_string = " matchinfo( f , '";
        uint8_t matchinfo_argdata = data_provider.ConsumeIntegral<uint8_t>();
        if (matchinfo_argdata == 0) {
          selector_string += "pcx"; // default value
        }
        /* to simplify I removed y, because it is almost the same as 'x'
         * =>https://www.sqlite.org/fts3.html#matchinfo */
        const char matchinfo_args[8] = {'p', 'c', 'n', 'a', 'l', 's', 'x', 'b'};
        size_t matchinfo_counter = 0;
        while (matchinfo_argdata > 0 &&
               matchinfo_counter < sizeof(matchinfo_args)) {
          if (matchinfo_argdata % 2) {
            selector_string += matchinfo_args[matchinfo_counter];
          }
          matchinfo_argdata >>= 1;
          matchinfo_counter++;
        }

        selector_string += "') ";
        break;
      }
      case 0x01:
        selector_string += " snippet(f) ";
        break;
      case 0x02:
        selector_string += " snippet(f, x'";
        selector_string += GetValueByType(&data_provider, ColumnType::kBlob);
        selector_string += "', x'";
        selector_string += GetValueByType(&data_provider, ColumnType::kBlob);
        selector_string += "', x'";
        selector_string += GetValueByType(&data_provider, ColumnType::kBlob);
        selector_string += "') ";
        break;
      case 0x03:
        selector_string += " offsets(f) ";
        break;
      case 0x04:
        selector_string += " * ";
        break;
      }

      op = "SELECT " + selector_string + " FROM f WHERE ";
      op += (op_16.select_operator_1 ? "a" : "b");
      uint16_t processor_byte =
          0; /* don't read a byte when it compares with = (eq)*/
      uint8_t current_bits = 0; /* 4 bits of data, maximum = 15*/
      switch (op_16.select_operator_2 % 4) {
      case 0: {
        bool need_string = true;
        uint8_t match_length = data_provider.ConsumeIntegralInRange<uint8_t>(
            1, 10); /* parts involved in match */
        op += " MATCH ";
        while (match_length && data_provider.remaining_bytes()) {
          if (need_string) {
            /* asterisk cases like MATCH 'a*' will be automatically covered in
             * this BLOB */
            op += GetValueByType(&data_provider, ColumnType::kBlob);
            need_string = false;
          } else {
            /*
                 ignoring brackets to simplify the logic since they can be
               converted into the equal forms.
                 eg: a AND (b OR c) == b OR c AND a
            */
            switch (data_provider.ConsumeIntegralInRange<uint8_t>(0, 3)) {
            case 0:
              op += " AND ";
              break;
            case 1:
              op += " OR ";
              break;
            case 2:
              op += " NEAR ";
              break;
            default:
            case 3:
              op += " NEAR/";
              op += GetValueByType(&data_provider, ColumnType::kShortNumber);
              op += " ";
              break;
            }
            need_string = true;
          }
          match_length--;
        }
        if (need_string) {
          /* asterisk cases like MATCH 'a*' will be automatically covered in
           * this BLOB */
          op += GetValueByType(&data_provider, ColumnType::kBlob);
          need_string = false;
        }
        op += ";";
      } break;
      case 1:
        processor_byte = data_provider.ConsumeIntegral<uint16_t>();
        op += " LIKE ";
        while (processor_byte) {
          current_bits = processor_byte % (1 << 4);
          processor_byte >>= 4;
          switch (current_bits) {
          case 0:
            op += "%";
            break;
          case 1:
            op += "_";
            break;
          case 2:
            op += " ";
            break;
          default:
            op += ('a' + current_bits);
            break;
          }
        }
        op += "';";
        break;
      case 2:
        op += " = 'a b';";
        break;
      case 3:
        processor_byte = data_provider.ConsumeIntegral<uint16_t>();
        op += " GLOB '";
        while (processor_byte) {
          current_bits = processor_byte % (1 << 4);
          processor_byte >>= 4;
          switch (current_bits) {
          case 0:
            op += "*";
            break;
          case 1:
            op += "?";
            break;
          case 2:
            op += "[AB]";
            break;
          case 3:
            op += "[0-9]";
            break;
          case 4:
            op += "[!A]";
            break;
          case 5:
            op += "[!3-5]";
            break;
          case 6:
            op += "\\";
            break;
          case 7:
            op += "/";
            break;
          default: // alphabets
            op += ('a' + current_bits);
            break;
          }
        }
        op += "';";
        break;
      } /* end of switch (op_16.select_operator_2 % 4) */
      break;
    }
    case 0x05: {
      std::string command;
      std::ostringstream ss;
      uint8_t command_operator =
          (op_16.select_operator_2 << 1) + op_16.select_operator_1;
      switch (command_operator % 5) {
      case 0x00:
        command = "optimize";
        break;
      case 0x01:
        command = "rebuild";
        break;
      case 0x02:
        command = "integrity-check";
        break;
      case 0x03:
        ss << GetValueByType(&data_provider, ColumnType::kShortNumber) << ','
           << GetValueByType(&data_provider, ColumnType::kShortNumber);
        command = "merge=";
        command += ss.str();
        break;
      case 0x04:
        ss << GetValueByType(&data_provider, ColumnType::kShortNumber);
        command = "automerge=";
        command += ss.str();
        break;
      }
      op = "INSERT INTO f(f) VALUES ('" + command + "');";
    } break;
    }

    RunSqlQuery(op, &exec_count);
  }
  /* Cleanup and return */

  sqlite3_close(g_database);

  return 0;
}

