/*
** 2012 Jan 11
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
/* TODO(shess): THIS MODULE IS STILL EXPERIMENTAL.  DO NOT USE IT. */
/* Implements a virtual table "recover" which can be used to recover
 * data from a corrupt table.  The table is walked manually, with
 * corrupt items skipped.  Additionally, any errors while reading will
 * be skipped.
 *
 * Given a table with this definition:
 *
 * CREATE TABLE Stuff (
 *   name TEXT PRIMARY KEY,
 *   value TEXT NOT NULL
 * );
 *
 * to recover the data from teh table, you could do something like:
 *
 * -- Attach another database, the original is not trustworthy.
 * ATTACH DATABASE '/tmp/db.db' AS rdb;
 * -- Create a new version of the table.
 * CREATE TABLE rdb.Stuff (
 *   name TEXT PRIMARY KEY,
 *   value TEXT NOT NULL
 * );
 * -- This will read the original table's data.
 * CREATE VIRTUAL TABLE temp.recover_Stuff using recover(
 *   main.Stuff,
 *   name TEXT STRICT NOT NULL,  -- only real TEXT data allowed
 *   value TEXT STRICT NOT NULL
 * );
 * -- Corruption means the UNIQUE constraint may no longer hold for
 * -- Stuff, so either OR REPLACE or OR IGNORE must be used.
 * INSERT OR REPLACE INTO rdb.Stuff (rowid, name, value )
 *   SELECT rowid, name, value FROM temp.recover_Stuff;
 * DROP TABLE temp.recover_Stuff;
 * DETACH DATABASE rdb;
 * -- Move db.db to replace original db in filesystem.
 *
 *
 * Usage
 *
 * Given the goal of dealing with corruption, it would not be safe to
 * create a recovery table in the database being recovered.  So
 * recovery tables must be created in the temp database.  They are not
 * appropriate to persist, in any case.  [As a bonus, sqlite_master
 * tables can be recovered.  Perhaps more cute than useful, though.]
 *
 * The parameters are a specifier for the table to read, and a column
 * definition for each bit of data stored in that table.  The named
 * table must be convertable to a root page number by reading the
 * sqlite_master table.  Bare table names are assumed to be in
 * database 0 ("main"), other databases can be specified in db.table
 * fashion.
 *
 * Column definitions are similar to BUT NOT THE SAME AS those
 * provided to CREATE statements:
 *  column-def: column-name [type-name [STRICT] [NOT NULL]]
 *  type-name: (ANY|ROWID|INTEGER|FLOAT|NUMERIC|TEXT|BLOB)
 *
 * Only those exact type names are accepted, there is no type
 * intuition.  The only constraints accepted are STRICT (see below)
 * and NOT NULL.  Anything unexpected will cause the create to fail.
 *
 * ANY is a convenience to indicate that manifest typing is desired.
 * It is equivalent to not specifying a type at all.  The results for
 * such columns will have the type of the data's storage.  The exposed
 * schema will contain no type for that column.
 *
 * ROWID is used for columns representing aliases to the rowid
 * (INTEGER PRIMARY KEY, with or without AUTOINCREMENT), to make the
 * concept explicit.  Such columns are actually stored as NULL, so
 * they cannot be simply ignored.  The exposed schema will be INTEGER
 * for that column.
 *
 * NOT NULL causes rows with a NULL in that column to be skipped.  It
 * also adds NOT NULL to the column in the exposed schema.  If the
 * table has ever had columns added using ALTER TABLE, then those
 * columns implicitly contain NULL for rows which have not been
 * updated.  [Workaround using COALESCE() in your SELECT statement.]
 *
 * The created table is read-only, with no indices.  Any SELECT will
 * be a full-table scan, returning each valid row read from the
 * storage of the backing table.  The rowid will be the rowid of the
 * row from the backing table.  "Valid" means:
 * - The cell metadata for the row is well-formed.  Mainly this means that
 *   the cell header info describes a payload of the size indicated by
 *   the cell's payload size.
 * - The cell does not run off the page.
 * - The cell does not overlap any other cell on the page.
 * - The cell contains doesn't contain too many columns.
 * - The types of the serialized data match the indicated types (see below).
 *
 *
 * Type affinity versus type storage.
 *
 * http://www.sqlite.org/datatype3.html describes SQLite's type
 * affinity system.  The system provides for automated coercion of
 * types in certain cases, transparently enough that many developers
 * do not realize that it is happening.  Importantly, it implies that
 * the raw data stored in the database may not have the obvious type.
 *
 * Differences between the stored data types and the expected data
 * types may be a signal of corruption.  This module makes some
 * allowances for automatic coercion.  It is important to be concious
 * of the difference between the schema exposed by the module, and the
 * data types read from storage.  The following table describes how
 * the module interprets things:
 *
 * type     schema   data                     STRICT
 * ----     ------   ----                     ------
 * ANY      <none>   any                      any
 * ROWID    INTEGER  n/a                      n/a
 * INTEGER  INTEGER  integer                  integer
 * FLOAT    FLOAT    integer or float         float
 * NUMERIC  NUMERIC  integer, float, or text  integer or float
 * TEXT     TEXT     text or blob             text
 * BLOB     BLOB     blob                     blob
 *
 * type is the type provided to the recover module, schema is the
 * schema exposed by the module, data is the acceptable types of data
 * decoded from storage, and STRICT is a modification of that.
 *
 * A very loose recovery system might use ANY for all columns, then
 * use the appropriate sqlite3_column_*() calls to coerce to expected
 * types.  This doesn't provide much protection if a page from a
 * different table with the same column count is linked into an
 * inappropriate btree.
 *
 * A very tight recovery system might use STRICT to enforce typing on
 * all columns, preferring to skip rows which are valid at the storage
 * level but don't contain the right types.  Note that FLOAT STRICT is
 * almost certainly not appropriate, since integral values are
 * transparently stored as integers, when that is more efficient.
 *
 * Another option is to use ANY for all columns and inspect each
 * result manually (using sqlite3_column_*).  This should only be
 * necessary in cases where developers have used manifest typing (test
 * to make sure before you decide that you aren't using manifest
 * typing!).
 *
 *
 * Caveats
 *
 * Leaf pages not referenced by interior nodes will not be found.
 *
 * Leaf pages referenced from interior nodes of other tables will not
 * be resolved.
 *
 * Rows referencing invalid overflow pages will be skipped.
 *
 * SQlite rows have a header which describes how to interpret the rest
 * of the payload.  The header can be valid in cases where the rest of
 * the record is actually corrupt (in the sense that the data is not
 * the intended data).  This can especially happen WRT overflow pages,
 * as lack of atomic updates between pages is the primary form of
 * corruption I have seen in the wild.
 */
/* TODO(shess): It might be useful to allow DEFAULT in types to
 * specify what to do for NULL when an ALTER TABLE case comes up.
 * Unfortunately, simply adding it to the exposed schema and using
 * sqlite3_result_null() does not cause the default to be generate.
 * Handling it ourselves seems hard, unfortunately.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Internal things that are used:
 * u32, u64, i64 types.
 * Btree, Pager, and DbPage structs.
 * DbPage.pData, .pPager, and .pgno
 * sqlite3 struct.
 * sqlite3BtreePager() and sqlite3BtreeGetPageSize()
 * sqlite3PagerAcquire() and sqlite3PagerUnref()
 * getVarint32() and getVarint().
 */
#include "sqliteInt.h"

/* For debugging. */
#if 0
#define FNENTRY() fprintf(stderr, "In %s\n", __FUNCTION__)
#else
#define FNENTRY()
#endif

/* Generic constants and helper functions. */

/* Accepted types are specified by a mask. */
#define MASK_ROWID (1<<0)
#define MASK_INTEGER (1<<1)
#define MASK_FLOAT (1<<2)
#define MASK_TEXT (1<<3)
#define MASK_BLOB (1<<4)
#define MASK_NULL (1<<5)

/* TODO(shess): In the future, these will be used more often.  For
 * now, just pretend they're useful.
 */

/* True if iSerialType refers to a blob. */
static int SerialTypeIsBlob(u64 iSerialType){
  assert( iSerialType>=12 );
  return (iSerialType%2)==0;
}

/* Returns true if the serialized type represented by iSerialType is
 * compatible with the given type mask.
 */
static int SerialTypeIsCompatible(u64 iSerialType, unsigned char mask){
  switch( iSerialType ){
    case 0  : return (mask&MASK_NULL)!=0;
    case 1  : return (mask&MASK_INTEGER)!=0;
    case 2  : return (mask&MASK_INTEGER)!=0;
    case 3  : return (mask&MASK_INTEGER)!=0;
    case 4  : return (mask&MASK_INTEGER)!=0;
    case 5  : return (mask&MASK_INTEGER)!=0;
    case 6  : return (mask&MASK_INTEGER)!=0;
    case 7  : return (mask&MASK_FLOAT)!=0;
    case 8  : return (mask&MASK_INTEGER)!=0;
    case 9  : return (mask&MASK_INTEGER)!=0;
    case 10 : assert( !"RESERVED TYPE"); return 0;
    case 11 : assert( !"RESERVED TYPE"); return 0;
  }
  return (mask&(SerialTypeIsBlob(iSerialType) ? MASK_BLOB : MASK_TEXT));
}

/* Versions of strdup() with return values appropriate for
 * sqlite3_free().  malloc.c has sqlite3DbStrDup()/NDup(), but those
 * need sqlite3DbFree(), which seems intrusive.
 */
static char *sqlite3_strndup(const char *z, unsigned n){
  if( z==NULL ){
    return NULL;
  }

  char *zNew = sqlite3_malloc(n+1);
  if( zNew!=NULL ){
    memcpy(zNew, z, n);
    zNew[n] = '\0';
  }
  return zNew;
}
static char *sqlite3_strdup(const char *z){
  if( z==NULL ){
    return NULL;
  }
  return sqlite3_strndup(z, strlen(z));
}

/* Fetch the page number of zTable in zDb from sqlite_master in zDb,
 * and put it in *piRootPage.
 */
static int getRootPage(sqlite3 *db, const char *zDb, const char *zTable,
                       unsigned *piRootPage){
  if( strcmp(zTable, "sqlite_master")==0 ){
    *piRootPage = 1;
    return SQLITE_OK;
  }

  char *zSql = sqlite3_mprintf("SELECT rootpage FROM %s.sqlite_master "
                               "WHERE type = 'table' AND tbl_name = %Q",
                               zDb, zTable);
  if( !zSql ){
    return SQLITE_NOMEM;
  }

  sqlite3_stmt *pStmt = 0;
  int rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  sqlite3_free(zSql);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* Require a result. */
  rc = sqlite3_step(pStmt);
  if( rc==SQLITE_DONE ){
    rc = SQLITE_CORRUPT;
  }else if( rc==SQLITE_ROW ){
    *piRootPage = sqlite3_column_int(pStmt, 0);

    /* Require only one result. */
    rc = sqlite3_step(pStmt);
    if( rc==SQLITE_DONE ){
      rc = SQLITE_OK;
    }else if( rc==SQLITE_ROW ){
      rc = SQLITE_CORRUPT;
    }
  }
  sqlite3_finalize(pStmt);
  return rc;
}

typedef struct Recover Recover;
struct Recover {
  sqlite3_vtab base;
  sqlite3 *db;                /* Host database connection */
  char *zDb;                  /* Database containing target table */
  char *zTable;               /* Target table */
  int nCols;                  /* Number of columns in target table */
  unsigned char *pTypes;      /* Types of columns in target table */
};

/* Internal helper for deleting the module. */
static void recoverRelease(Recover *pRecover){
  sqlite3_free(pRecover->zDb);
  sqlite3_free(pRecover->zTable);
  sqlite3_free(pRecover->pTypes);
  memset(pRecover, 0xA5, sizeof(*pRecover));
  sqlite3_free(pRecover);
}

/* Helper function for initializing the module.  Forward-declared so
 * recoverCreate() and recoverConnect() can see it.
 */
static int recoverInit(
  sqlite3 *, void *, int, const char *const*, sqlite3_vtab **, char **
);

static int recoverCreate(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  FNENTRY();
  return recoverInit(db, pAux, argc, argv, ppVtab, pzErr);
}

/* This should never be called. */
static int recoverConnect(
  sqlite3 *db,
  void *pAux,
  int argc, const char *const*argv,
  sqlite3_vtab **ppVtab,
  char **pzErr
){
  FNENTRY();
  return recoverInit(db, pAux, argc, argv, ppVtab, pzErr);
}

/* No indices supported. */
static int recoverBestIndex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo){
  FNENTRY();
  return SQLITE_OK;
}

/* Logically, this should never be called. */
static int recoverDisconnect(sqlite3_vtab *pVtab){
  FNENTRY();
  recoverRelease((Recover*)pVtab);
  return SQLITE_OK;
}

static int recoverDestroy(sqlite3_vtab *pVtab){
  FNENTRY();
  recoverRelease((Recover*)pVtab);
  return SQLITE_OK;
}

typedef struct RecoverCursor RecoverCursor;
struct RecoverCursor {
  sqlite3_vtab_cursor base;
  i64 iRowid;  /* TODO(shess): Implement for real. */
  int bEOF;
};

static int recoverOpen(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor){
  FNENTRY();

  Recover *pRecover = (Recover*)pVTab;

  unsigned iRootPage = 0;
  int rc = getRootPage(pRecover->db, pRecover->zDb, pRecover->zTable,
                       &iRootPage);
  if( rc!=SQLITE_OK ){
    return rc;
  }

  /* TODO(shess): Implement some real stuff in here. */

  RecoverCursor *pCursor = sqlite3_malloc(sizeof(RecoverCursor));
  if( !pCursor ){
    return SQLITE_NOMEM;
  }
  memset(pCursor, 0, sizeof(*pCursor));
  pCursor->base.pVtab = pVTab;
  pCursor->iRowid = 0;

  *ppCursor = (sqlite3_vtab_cursor*)pCursor;
  return SQLITE_OK;
}

static int recoverClose(sqlite3_vtab_cursor *cur){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)cur;
  memset(pCursor, 0xA5, sizeof(*pCursor));
  sqlite3_free(cur);
  return SQLITE_OK;
}

/* TODO(shess): Some data for purposes of mocking things.  Will go
 * away.
 */
static struct {
  u64 iColType;
  const char *zTypeName;
} gMockData[] = {
  { 0, "NULL"},
  { 1, "INTEGER"},
  { 7, "FLOAT"},
  { 13, "TEXT"},
  { 12, "BLOB"},
};

static int recoverNext(sqlite3_vtab_cursor *pVtabCursor){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  Recover *pRecover = (Recover*)pCursor->base.pVtab;

  /* iRowid is 1-base, gMockData is 0-based. */
  while( pCursor->iRowid<ArraySize(gMockData) ){
    pCursor->iRowid++;
    if( SerialTypeIsCompatible(gMockData[pCursor->iRowid-1].iColType,
                               pRecover->pTypes[pRecover->nCols-1]) ){
      return SQLITE_OK;
    }
  }
  pCursor->bEOF = 1;
  return SQLITE_OK;
}

static int recoverFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStr,
  int argc, sqlite3_value **argv
){
  FNENTRY();
  return recoverNext(pVtabCursor);
}

static int recoverEof(sqlite3_vtab_cursor *pVtabCursor){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  return pCursor->bEOF;
}

static int recoverColumn(sqlite3_vtab_cursor *cur, sqlite3_context *ctx, int i){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)cur;
  Recover *pRecover = (Recover*)pCursor->base.pVtab;

  if( i>=pRecover->nCols ){
    return SQLITE_ERROR;
  }

  /* ROWID alias. */
  if( (pRecover->pTypes[i]&MASK_ROWID) ){
    sqlite3_result_int64(ctx, pCursor->iRowid);
    return SQLITE_OK;
  }

  /* TODO(shess): Replace this with real code. */
  if( pCursor->iRowid<1 || pCursor->iRowid>ArraySize(gMockData)+1 ){
    return SQLITE_ERROR;
  }
  if( i==pRecover->nCols-2 ){
    sqlite3_result_text(ctx, gMockData[pCursor->iRowid-1].zTypeName, -1,
                        SQLITE_STATIC);
  }else if( i==pRecover->nCols-1 ){
    switch( gMockData[pCursor->iRowid-1].iColType ){
      case 0 : sqlite3_result_null(ctx); break;
      case 1 : sqlite3_result_int(ctx, 17); break;
      case 7 : sqlite3_result_double(ctx, 3.1415927); break;
      case 13 :
        sqlite3_result_text(ctx, "This is text", -1, SQLITE_STATIC);
        break;
      case 12 :
        sqlite3_result_blob(ctx, "This is a blob", 14, SQLITE_STATIC);
        break;
    }
  }
  return SQLITE_OK;
}

static int recoverRowid(sqlite3_vtab_cursor *pVtabCursor, sqlite_int64 *pRowid){
  FNENTRY();
  RecoverCursor *pCursor = (RecoverCursor*)pVtabCursor;
  *pRowid = pCursor->iRowid;
  return SQLITE_OK;
}

static sqlite3_module recoverModule = {
  0,                         /* iVersion */
  recoverCreate,             /* xCreate - create a table */
  recoverConnect,            /* xConnect - connect to an existing table */
  recoverBestIndex,          /* xBestIndex - Determine search strategy */
  recoverDisconnect,         /* xDisconnect - Disconnect from a table */
  recoverDestroy,            /* xDestroy - Drop a table */
  recoverOpen,               /* xOpen - open a cursor */
  recoverClose,              /* xClose - close a cursor */
  recoverFilter,             /* xFilter - configure scan constraints */
  recoverNext,               /* xNext - advance a cursor */
  recoverEof,                /* xEof */
  recoverColumn,             /* xColumn - read data */
  recoverRowid,              /* xRowid - read data */
  0,                         /* xUpdate - write data */
  0,                         /* xBegin - begin transaction */
  0,                         /* xSync - sync transaction */
  0,                         /* xCommit - commit transaction */
  0,                         /* xRollback - rollback transaction */
  0,                         /* xFindFunction - function overloading */
  0,                         /* xRename - rename the table */
};

int recoverVtableInit(sqlite3 *db){
  return sqlite3_create_module_v2(db, "recover", &recoverModule, NULL, 0);
}

/* This section of code is for parsing the create input and
 * initializing the module.
 */

/* Find the next word in zText and place the endpoints in pzWord*.
 * Returns true if the word is non-empty.  "Word" is defined as
 * alphanumeric plus '_' at this time.
 */
static int findWord(const char *zText,
                    const char **pzWordStart, const char **pzWordEnd){
  while( isspace(*zText) ){
    zText++;
  }
  *pzWordStart = zText;
  while( isalnum(*zText) || *zText=='_' ){
    zText++;
  }
  int r = zText>*pzWordStart;  /* In case pzWordStart==pzWordEnd */
  *pzWordEnd = zText;
  return r;
}

/* Return true if the next word in zText is zWord, also setting
 * *pzContinue to the character after the word.
 */
static int expectWord(const char *zText, const char *zWord,
                      const char **pzContinue){
  const char *zWordStart, *zWordEnd;
  if( findWord(zText, &zWordStart, &zWordEnd) &&
      strncasecmp(zWord, zWordStart, zWordEnd - zWordStart)==0 ){
    *pzContinue = zWordEnd;
    return 1;
  }
  return 0;
}

/* Parse the name and type information out of parameter.  In case of
 * success, *pzNameStart/End contain the name of the column,
 * *pzTypeStart/End contain the top-level type, and *pTypeMask has the
 * type mask to use for the column.
 */
static int findNameAndType(const char *parameter,
                           const char **pzNameStart, const char **pzNameEnd,
                           const char **pzTypeStart, const char **pzTypeEnd,
                           unsigned char *pTypeMask){
  if( !findWord(parameter, pzNameStart, pzNameEnd) ){
    return SQLITE_MISUSE;
  }

  /* Manifest typing, accept any storage type. */
  if( !findWord(*pzNameEnd, pzTypeStart, pzTypeEnd) ){
    *pzTypeEnd = *pzTypeStart = "";
    *pTypeMask = MASK_INTEGER | MASK_FLOAT | MASK_BLOB | MASK_TEXT | MASK_NULL;
    return SQLITE_OK;
  }

  /* strictMask is used for STRICT, strictMask|otherMask if STRICT is
   * not supplied.  zReplace provides an alternate type to expose to
   * the caller.
   */
  static struct {
    const char *zName;
    unsigned char strictMask;
    unsigned char otherMask;
    const char *zReplace;
  } kTypeInfo[] = {
    { "ANY",
      MASK_INTEGER | MASK_FLOAT | MASK_BLOB | MASK_TEXT | MASK_NULL,
      0, "",
    },
    { "ROWID",   MASK_INTEGER | MASK_ROWID,             0, "INTEGER", },
    { "INTEGER", MASK_INTEGER | MASK_NULL,              0, NULL, },
    { "FLOAT",   MASK_FLOAT | MASK_NULL,                MASK_INTEGER, NULL, },
    { "NUMERIC", MASK_INTEGER | MASK_FLOAT | MASK_NULL, MASK_TEXT, NULL, },
    { "TEXT",    MASK_TEXT | MASK_NULL,                 MASK_BLOB, NULL, },
    { "BLOB",    MASK_BLOB | MASK_NULL,                 0, NULL, },
  };

  unsigned i, nNameLen = *pzTypeEnd - *pzTypeStart;
  for( i=0; i<ArraySize(kTypeInfo); ++i ){
    if( strncasecmp(kTypeInfo[i].zName, *pzTypeStart, nNameLen)==0 ){
      break;
    }
  }
  if( i==ArraySize(kTypeInfo) ){
    return SQLITE_MISUSE;
  }

  const char *zEnd = *pzTypeEnd;
  int bStrict = 0;
  if( expectWord(zEnd, "STRICT", &zEnd) ){
    /* TODO(shess): Ick.  But I don't want another single-purpose
     * flag, either.
     */
    if( kTypeInfo[i].zReplace && !kTypeInfo[i].zReplace[0] ){
      return SQLITE_MISUSE;
    }
    bStrict = 1;
  }

  int bNotNull = 0;
  if( expectWord(zEnd, "NOT", &zEnd) ){
    if( expectWord(zEnd, "NULL", &zEnd) ){
      bNotNull = 1;
    }else{
      /* Anything other than NULL after NOT is an error. */
      return SQLITE_MISUSE;
    }
  }

  /* Anything else is an error. */
  const char *zDummy;
  if( findWord(zEnd, &zDummy, &zDummy) ){
    return SQLITE_MISUSE;
  }

  *pTypeMask = kTypeInfo[i].strictMask;
  if( !bStrict ){
    *pTypeMask |= kTypeInfo[i].otherMask;
  }
  if( bNotNull ){
    *pTypeMask &= ~MASK_NULL;
  }
  if( kTypeInfo[i].zReplace ){
    *pzTypeStart = kTypeInfo[i].zReplace;
    *pzTypeEnd = *pzTypeStart + strlen(*pzTypeStart);
  }
  return SQLITE_OK;
}

/* Parse the arguments, placing type masks in *pTypes and the exposed
 * schema in *pzCreateSql (for sqlite3_declare_vtab).
 */
static int ParseColumnsAndGenerateCreate(int nCols, const char *const *pCols,
                                         char **pzCreateSql,
                                         unsigned char *pTypes,
                                         char **pzErr){
  char *zCreateSql = sqlite3_mprintf("CREATE TABLE x(");
  if( !zCreateSql ){
    return SQLITE_NOMEM;
  }

  unsigned i;
  for( i=0; i<nCols; i++ ){
    const char *zNameStart, *zNameEnd;
    const char *zTypeStart, *zTypeEnd;
    int rc = findNameAndType(pCols[i],
                             &zNameStart, &zNameEnd,
                             &zTypeStart, &zTypeEnd,
                             &pTypes[i]);
    if( rc!=SQLITE_OK ){
      *pzErr = sqlite3_mprintf("unable to parse column %d", i);
      sqlite3_free(zCreateSql);
      return rc;
    }

    const char *zNotNull = "";
    if( !(pTypes[i]&MASK_NULL) ){
      zNotNull = " NOT NULL";
    }

    /* Add name and type to the create statement. */
    const char *zSep = (i < nCols - 1 ? ", " : ")");
    zCreateSql = sqlite3_mprintf("%z%.*s %.*s%s%s",
                                 zCreateSql,
                                 zNameEnd - zNameStart, zNameStart,
                                 zTypeEnd - zTypeStart, zTypeStart,
                                 zNotNull, zSep);
    if( !zCreateSql ){
      return SQLITE_NOMEM;
    }
  }

  *pzCreateSql = zCreateSql;
  return SQLITE_OK;
}

/* Helper function for initializing the module. */
/* TODO(shess): Since connect isn't supported, could inline into
 * recoverCreate().
 */
/* TODO(shess): Explore cases where it would make sense to set *pzErr. */
static int recoverInit(
  sqlite3 *db,                        /* Database connection */
  void *pAux,                         /* unused */
  int argc, const char *const*argv,   /* Parameters to CREATE TABLE statement */
  sqlite3_vtab **ppVtab,              /* OUT: New virtual table */
  char **pzErr                        /* OUT: Error message, if any */
){
  /* argv[0] module name
   * argv[1] db name for virtual table
   * argv[2] virtual table name
   * argv[3] backing table name
   * argv[4] columns
   */
  const int kTypeCol = 4;
  int nCols = argc - kTypeCol;

  /* Require to be in the temp database. */
  if( strcasecmp(argv[1], "temp")!=0 ){
    *pzErr = sqlite3_mprintf("recover table must be in temp database");
    return SQLITE_MISUSE;
  }

  /* Need the backing table and at least one column. */
  if( nCols<1 ){
    *pzErr = sqlite3_mprintf("no columns specified");
    return SQLITE_MISUSE;
  }

  Recover *pRecover = sqlite3_malloc(sizeof(Recover));
  if( !pRecover ){
    return SQLITE_NOMEM;
  }
  memset(pRecover, 0, sizeof(*pRecover));
  pRecover->base.pModule = &recoverModule;
  pRecover->db = db;

  /* Parse out db.table, assuming main if no dot. */
  char *zDot = strchr(argv[3], '.');
  if( !zDot ){
    pRecover->zDb = sqlite3_strdup(db->aDb[0].zName);
    pRecover->zTable = sqlite3_strdup(argv[3]);
  }else if( zDot>argv[3] && zDot[1]!='\0' ){
    pRecover->zDb = sqlite3_strndup(argv[3], zDot - argv[3]);
    pRecover->zTable = sqlite3_strdup(zDot + 1);
  }else{
    /* ".table" or "db." not allowed. */
    *pzErr = sqlite3_mprintf("ill-formed table specifier");
    recoverRelease(pRecover);
    return SQLITE_ERROR;
  }

  pRecover->nCols = nCols;
  pRecover->pTypes = sqlite3_malloc(pRecover->nCols);
  if( !pRecover->zDb || !pRecover->zTable || !pRecover->pTypes ){
    recoverRelease(pRecover);
    return SQLITE_NOMEM;
  }

  /* Require the backing table to exist. */
  /* TODO(shess): Be more pedantic about the form of the descriptor
   * string.  This already fails for poorly-formed strings, simply
   * because there won't be a root page, but it would make more sense
   * to be explicit.
   */
  unsigned iRootPage;
  int rc = getRootPage(pRecover->db, pRecover->zDb, pRecover->zTable,
                       &iRootPage);
  if( rc!=SQLITE_OK ){
    *pzErr = sqlite3_mprintf("unable to find backing table");
    recoverRelease(pRecover);
    return rc;
  }

  /* Parse the column definitions. */
  char *zCreateSql;
  rc = ParseColumnsAndGenerateCreate(nCols, argv + kTypeCol,
                                     &zCreateSql, pRecover->pTypes, pzErr);
  if( rc!=SQLITE_OK ){
    recoverRelease(pRecover);
    return rc;
  }

  rc = sqlite3_declare_vtab(db, zCreateSql);
  sqlite3_free(zCreateSql);
  if( rc!=SQLITE_OK ){
    recoverRelease(pRecover);
    return rc;
  }

  *ppVtab = (sqlite3_vtab *)pRecover;
  return SQLITE_OK;
}
