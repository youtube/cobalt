// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

#include <nss.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>
#include <prtime.h>
#include <pk11pub.h>
#include <secmod.h>

#if defined(OS_LINUX)
#include <linux/nfs_fs.h>
#include <sys/vfs.h>
#endif

#include <vector>

#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "crypto/scoped_nss_types.h"

// USE_NSS means we use NSS for everything crypto-related.  If USE_NSS is not
// defined, such as on Mac and Windows, we use NSS for SSL only -- we don't
// use NSS for crypto or certificate verification, and we don't use the NSS
// certificate and key databases.
#if defined(USE_NSS)
#include "base/synchronization/lock.h"
#include "crypto/crypto_module_blocking_password_delegate.h"
#endif  // defined(USE_NSS)

namespace crypto {

namespace {

#if defined(OS_CHROMEOS)
const char kNSSDatabaseName[] = "Real NSS database";

// Constants for loading opencryptoki.
const char kOpencryptokiModuleName[] = "opencryptoki";
const char kOpencryptokiPath[] = "/usr/lib/opencryptoki/libopencryptoki.so";

// Fake certificate authority database used for testing.
static const FilePath::CharType kReadOnlyCertDB[] =
    FILE_PATH_LITERAL("/etc/fake_root_ca/nssdb");
#endif  // defined(OS_CHROMEOS)

std::string GetNSSErrorMessage() {
  std::string result;
  if (PR_GetErrorTextLength()) {
    scoped_array<char> error_text(new char[PR_GetErrorTextLength() + 1]);
    PRInt32 copied = PR_GetErrorText(error_text.get());
    result = std::string(error_text.get(), copied);
  } else {
    result = StringPrintf("NSS error code: %d", PR_GetError());
  }
  return result;
}

#if defined(USE_NSS)
FilePath GetDefaultConfigDirectory() {
  FilePath dir = file_util::GetHomeDir();
  if (dir.empty()) {
    LOG(ERROR) << "Failed to get home directory.";
    return dir;
  }
  dir = dir.AppendASCII(".pki").AppendASCII("nssdb");
  if (!file_util::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create ~/.pki/nssdb directory.";
    dir.clear();
  }
  return dir;
}

// On non-chromeos platforms, return the default config directory.
// On chromeos, return a read-only directory with fake root CA certs for testing
// (which will not exist on non-testing images).  These root CA certs are used
// by the local Google Accounts server mock we use when testing our login code.
// If this directory is not present, NSS_Init() will fail.  It is up to the
// caller to failover to NSS_NoDB_Init() at that point.
FilePath GetInitialConfigDirectory() {
#if defined(OS_CHROMEOS)
  return FilePath(kReadOnlyCertDB);
#else
  return GetDefaultConfigDirectory();
#endif  // defined(OS_CHROMEOS)
}

// This callback for NSS forwards all requests to a caller-specified
// CryptoModuleBlockingPasswordDelegate object.
char* PKCS11PasswordFunc(PK11SlotInfo* slot, PRBool retry, void* arg) {
#if defined(OS_CHROMEOS)
  // If we get asked for a password for the TPM, then return the
  // well known password we use, as long as the TPM slot has been
  // initialized.
  if (crypto::IsTPMTokenReady()) {
    std::string token_name;
    std::string user_pin;
    crypto::GetTPMTokenInfo(&token_name, &user_pin);
    if (PK11_GetTokenName(slot) == token_name)
      return PORT_Strdup(user_pin.c_str());
  }
#endif
  crypto::CryptoModuleBlockingPasswordDelegate* delegate =
      reinterpret_cast<crypto::CryptoModuleBlockingPasswordDelegate*>(arg);
  if (delegate) {
    bool cancelled = false;
    std::string password = delegate->RequestPassword(PK11_GetTokenName(slot),
                                                     retry != PR_FALSE,
                                                     &cancelled);
    if (cancelled)
      return NULL;
    char* result = PORT_Strdup(password.c_str());
    password.replace(0, password.size(), password.size(), 0);
    return result;
  }
  DLOG(ERROR) << "PK11 password requested with NULL arg";
  return NULL;
}

// NSS creates a local cache of the sqlite database if it detects that the
// filesystem the database is on is much slower than the local disk.  The
// detection doesn't work with the latest versions of sqlite, such as 3.6.22
// (NSS bug https://bugzilla.mozilla.org/show_bug.cgi?id=578561).  So we set
// the NSS environment variable NSS_SDB_USE_CACHE to "yes" to override NSS's
// detection when database_dir is on NFS.  See http://crbug.com/48585.
//
// TODO(wtc): port this function to other USE_NSS platforms.  It is defined
// only for OS_LINUX simply because the statfs structure is OS-specific.
//
// Because this function sets an environment variable it must be run before we
// go multi-threaded.
void UseLocalCacheOfNSSDatabaseIfNFS(const FilePath& database_dir) {
#if defined(OS_LINUX)
  struct statfs buf;
  if (statfs(database_dir.value().c_str(), &buf) == 0) {
    if (buf.f_type == NFS_SUPER_MAGIC) {
      scoped_ptr<base::Environment> env(base::Environment::Create());
      const char* use_cache_env_var = "NSS_SDB_USE_CACHE";
      if (!env->HasVar(use_cache_env_var))
        env->SetVar(use_cache_env_var, "yes");
    }
  }
#endif  // defined(OS_LINUX)
}

// A helper class that acquires the SECMOD list read lock while the
// AutoSECMODListReadLock is in scope.
class AutoSECMODListReadLock {
 public:
  AutoSECMODListReadLock()
      : lock_(SECMOD_GetDefaultModuleListLock()) {
    SECMOD_GetReadLock(lock_);
  }

  ~AutoSECMODListReadLock() {
    SECMOD_ReleaseReadLock(lock_);
  }

 private:
  SECMODListLock* lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoSECMODListReadLock);
};

PK11SlotInfo* FindSlotWithTokenName(const std::string& token_name) {
  AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != NULL; item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (PK11_GetTokenName(slot) == token_name)
        return PK11_ReferenceSlot(slot);
    }
  }
  return NULL;
}

#endif  // defined(USE_NSS)

// A singleton to initialize/deinitialize NSPR.
// Separate from the NSS singleton because we initialize NSPR on the UI thread.
// Now that we're leaking the singleton, we could merge back with the NSS
// singleton.
class NSPRInitSingleton {
 private:
  friend struct base::DefaultLazyInstanceTraits<NSPRInitSingleton>;

  NSPRInitSingleton() {
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  }

  // NOTE(willchan): We don't actually execute this code since we leak NSS to
  // prevent non-joinable threads from using NSS after it's already been shut
  // down.
  ~NSPRInitSingleton() {
    PL_ArenaFinish();
    PRStatus prstatus = PR_Cleanup();
    if (prstatus != PR_SUCCESS) {
      LOG(ERROR) << "PR_Cleanup failed; was NSPR initialized on wrong thread?";
    }
  }
};

base::LazyInstance<NSPRInitSingleton,
                   base::LeakyLazyInstanceTraits<NSPRInitSingleton> >
    g_nspr_singleton(base::LINKER_INITIALIZED);

class NSSInitSingleton {
 public:
#if defined(OS_CHROMEOS)
  void OpenPersistentNSSDB() {
    if (!chromeos_user_logged_in_) {
      // GetDefaultConfigDirectory causes us to do blocking IO on UI thread.
      // Temporarily allow it until we fix http://crbug.com/70119
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      chromeos_user_logged_in_ = true;

      // This creates another DB slot in NSS that is read/write, unlike
      // the fake root CA cert DB and the "default" crypto key
      // provider, which are still read-only (because we initialized
      // NSS before we had a cryptohome mounted).
      software_slot_ = OpenUserDB(GetDefaultConfigDirectory(),
                                  kNSSDatabaseName);
    }
  }

  void EnableTPMTokenForNSS(TPMTokenInfoDelegate* info_delegate) {
    CHECK(info_delegate);
    tpm_token_info_delegate_.reset(info_delegate);
    // Try to load once to avoid jank later.  Ignore the return value,
    // because if it fails we will try again later.
    EnsureTPMTokenReady();
  }

  void GetTPMTokenInfo(std::string* token_name, std::string* user_pin) {
    tpm_token_info_delegate_->GetTokenInfo(token_name, user_pin);
  }

  bool IsTPMTokenReady() {
    return tpm_slot_ != NULL;
  }

  PK11SlotInfo* GetTPMSlot() {
    std::string token_name;
    GetTPMTokenInfo(&token_name, NULL);
    return FindSlotWithTokenName(token_name);
  }
#endif  // defined(OS_CHROMEOS)


  bool OpenTestNSSDB(const FilePath& path, const char* description) {
    test_slot_ = OpenUserDB(path, description);
    return !!test_slot_;
  }

  void CloseTestNSSDB() {
    if (test_slot_) {
      SECStatus status = SECMOD_CloseUserDB(test_slot_);
      if (status != SECSuccess)
        LOG(ERROR) << "SECMOD_CloseUserDB failed: " << PORT_GetError();
      PK11_FreeSlot(test_slot_);
      test_slot_ = NULL;
    }
  }

  PK11SlotInfo* GetPublicNSSKeySlot() {
    if (test_slot_)
      return PK11_ReferenceSlot(test_slot_);
    if (software_slot_)
      return PK11_ReferenceSlot(software_slot_);
    return PK11_GetInternalKeySlot();
  }

  PK11SlotInfo* GetPrivateNSSKeySlot() {
    if (test_slot_)
      return PK11_ReferenceSlot(test_slot_);

#if defined(OS_CHROMEOS)
    // Make sure that if EnableTPMTokenForNSS has been called that we
    // have successfully loaded opencryptoki.
    if (tpm_token_info_delegate_.get() != NULL) {
      if (EnsureTPMTokenReady()) {
        return PK11_ReferenceSlot(tpm_slot_);
      } else {
        // If we were supposed to get the hardware token, but were
        // unable to, return NULL rather than fall back to sofware.
        return NULL;
      }
    }
#endif
    // If we weren't supposed to enable the TPM for NSS, then return
    // the software slot.
    if (software_slot_)
      return PK11_ReferenceSlot(software_slot_);
    return PK11_GetInternalKeySlot();
  }

#if defined(USE_NSS)
  base::Lock* write_lock() {
    return &write_lock_;
  }
#endif  // defined(USE_NSS)

  // This method is used to force NSS to be initialized without a DB.
  // Call this method before NSSInitSingleton() is constructed.
  static void ForceNoDBInit() {
    force_nodb_init_ = true;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<NSSInitSingleton>;

  NSSInitSingleton()
      : opencryptoki_module_(NULL),
        software_slot_(NULL),
        test_slot_(NULL),
        tpm_slot_(NULL),
        root_(NULL),
        chromeos_user_logged_in_(false) {
    EnsureNSPRInit();

    // We *must* have NSS >= 3.12.3.  See bug 26448.
    COMPILE_ASSERT(
        (NSS_VMAJOR == 3 && NSS_VMINOR == 12 && NSS_VPATCH >= 3) ||
        (NSS_VMAJOR == 3 && NSS_VMINOR > 12) ||
        (NSS_VMAJOR > 3),
        nss_version_check_failed);
    // Also check the run-time NSS version.
    // NSS_VersionCheck is a >= check, not strict equality.
    if (!NSS_VersionCheck("3.12.3")) {
      // It turns out many people have misconfigured NSS setups, where
      // their run-time NSPR doesn't match the one their NSS was compiled
      // against.  So rather than aborting, complain loudly.
      LOG(ERROR) << "NSS_VersionCheck(\"3.12.3\") failed.  "
                    "We depend on NSS >= 3.12.3, and this error is not fatal "
                    "only because many people have busted NSS setups (for "
                    "example, using the wrong version of NSPR). "
                    "Please upgrade to the latest NSS and NSPR, and if you "
                    "still get this error, contact your distribution "
                    "maintainer.";
    }

    SECStatus status = SECFailure;
    bool nodb_init = force_nodb_init_;

#if !defined(USE_NSS)
    // Use the system certificate store, so initialize NSS without database.
    nodb_init = true;
#endif

    if (nodb_init) {
      status = NSS_NoDB_Init(NULL);
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS without a persistent "
                      "database: " << GetNSSErrorMessage();
      }
    } else {
#if defined(USE_NSS)
      FilePath database_dir = GetInitialConfigDirectory();
      if (!database_dir.empty()) {
        // This duplicates the work which should have been done in
        // EarlySetupForNSSInit. However, this function is idempotent so
        // there's no harm done.
        UseLocalCacheOfNSSDatabaseIfNFS(database_dir);

        // Initialize with a persistent database (likely, ~/.pki/nssdb).
        // Use "sql:" which can be shared by multiple processes safely.
        std::string nss_config_dir =
            StringPrintf("sql:%s", database_dir.value().c_str());
#if defined(OS_CHROMEOS)
        status = NSS_Init(nss_config_dir.c_str());
#else
        status = NSS_InitReadWrite(nss_config_dir.c_str());
#endif
        if (status != SECSuccess) {
          LOG(ERROR) << "Error initializing NSS with a persistent "
                        "database (" << nss_config_dir
                     << "): " << GetNSSErrorMessage();
        }
      }
      if (status != SECSuccess) {
        VLOG(1) << "Initializing NSS without a persistent database.";
        status = NSS_NoDB_Init(NULL);
        if (status != SECSuccess) {
          LOG(ERROR) << "Error initializing NSS without a persistent "
                        "database: " << GetNSSErrorMessage();
          return;
        }
      }

      PK11_SetPasswordFunc(PKCS11PasswordFunc);

      // If we haven't initialized the password for the NSS databases,
      // initialize an empty-string password so that we don't need to
      // log in.
      PK11SlotInfo* slot = PK11_GetInternalKeySlot();
      if (slot) {
        // PK11_InitPin may write to the keyDB, but no other thread can use NSS
        // yet, so we don't need to lock.
        if (PK11_NeedUserInit(slot))
          PK11_InitPin(slot, NULL, NULL);
        PK11_FreeSlot(slot);
      }

      root_ = InitDefaultRootCerts();
#endif  // defined(USE_NSS)
    }
  }

  // NOTE(willchan): We don't actually execute this code since we leak NSS to
  // prevent non-joinable threads from using NSS after it's already been shut
  // down.
  ~NSSInitSingleton() {
    if (tpm_slot_) {
      PK11_FreeSlot(tpm_slot_);
      tpm_slot_ = NULL;
    }
    if (software_slot_) {
      SECMOD_CloseUserDB(software_slot_);
      PK11_FreeSlot(software_slot_);
      software_slot_ = NULL;
    }
    CloseTestNSSDB();
    if (root_) {
      SECMOD_UnloadUserModule(root_);
      SECMOD_DestroyModule(root_);
      root_ = NULL;
    }
    if (opencryptoki_module_) {
      SECMOD_UnloadUserModule(opencryptoki_module_);
      SECMOD_DestroyModule(opencryptoki_module_);
      opencryptoki_module_ = NULL;
    }

    SECStatus status = NSS_Shutdown();
    if (status != SECSuccess) {
      // We VLOG(1) because this failure is relatively harmless (leaking, but
      // we're shutting down anyway).
      VLOG(1) << "NSS_Shutdown failed; see http://crbug.com/4609";
    }
  }

#if defined(USE_NSS)
  // Load nss's built-in root certs.
  SECMODModule* InitDefaultRootCerts() {
    SECMODModule* root = LoadModule("Root Certs", "libnssckbi.so", NULL);
    if (root)
      return root;

    // Aw, snap.  Can't find/load root cert shared library.
    // This will make it hard to talk to anybody via https.
    NOTREACHED();
    return NULL;
  }

  // Load the given module for this NSS session.
  SECMODModule* LoadModule(const char* name,
                           const char* library_path,
                           const char* params) {
    std::string modparams = StringPrintf(
        "name=\"%s\" library=\"%s\" %s",
        name, library_path, params ? params : "");

    // Shouldn't need to const_cast here, but SECMOD doesn't properly
    // declare input string arguments as const.  Bug
    // https://bugzilla.mozilla.org/show_bug.cgi?id=642546 was filed
    // on NSS codebase to address this.
    SECMODModule* module = SECMOD_LoadUserModule(
        const_cast<char*>(modparams.c_str()), NULL, PR_FALSE);
    if (!module) {
      LOG(ERROR) << "Error loading " << name << " module into NSS: "
                 << GetNSSErrorMessage();
      return NULL;
    }
    return module;
  }
#endif

  static PK11SlotInfo* OpenUserDB(const FilePath& path,
                                  const char* description) {
    const std::string modspec =
        StringPrintf("configDir='sql:%s' tokenDescription='%s'",
                     path.value().c_str(), description);
    PK11SlotInfo* db_slot = SECMOD_OpenUserDB(modspec.c_str());
    if (db_slot) {
      if (PK11_NeedUserInit(db_slot))
        PK11_InitPin(db_slot, NULL, NULL);
    }
    else {
      LOG(ERROR) << "Error opening persistent database (" << modspec
                 << "): " << GetNSSErrorMessage();
    }
    return db_slot;
  }

#if defined(OS_CHROMEOS)
  // This is called whenever we want to make sure opencryptoki is
  // properly loaded, because it can fail shortly after the initial
  // login while the PINs are being initialized, and we want to retry
  // if this happens.
  bool EnsureTPMTokenReady() {
    // If EnableTPMTokenForNSS hasn't been called, or if everything is
    // already initialized, then this call succeeds.
    if (tpm_token_info_delegate_.get() == NULL ||
        (opencryptoki_module_ && tpm_slot_)) {
      return true;
    }

    if (tpm_token_info_delegate_->IsTokenReady()) {
      // This tries to load the opencryptoki module so NSS can talk to
      // the hardware TPM.
      if (!opencryptoki_module_) {
        opencryptoki_module_ = LoadModule(
            kOpencryptokiModuleName,
            kOpencryptokiPath,
            // trustOrder=100 -- means it'll select this as the most
            //   trusted slot for the mechanisms it provides.
            // slotParams=... -- selects RSA as the only mechanism, and only
            //   asks for the password when necessary (instead of every
            //   time, or after a timeout).
            "trustOrder=100 slotParams=(1={slotFlags=[RSA] askpw=only})");
      }
      if (opencryptoki_module_) {
        // If this gets set, then we'll use the TPM for certs with
        // private keys, otherwise we'll fall back to the software
        // implementation.
        tpm_slot_ = GetTPMSlot();
        return tpm_slot_ != NULL;
      }
    }
    return false;
  }
#endif

  // If this is set to true NSS is forced to be initialized without a DB.
  static bool force_nodb_init_;

#if defined(OS_CHROMEOS)
  scoped_ptr<TPMTokenInfoDelegate> tpm_token_info_delegate_;
#endif

  SECMODModule* opencryptoki_module_;
  PK11SlotInfo* software_slot_;
  PK11SlotInfo* test_slot_;
  PK11SlotInfo* tpm_slot_;
  SECMODModule* root_;
  bool chromeos_user_logged_in_;
#if defined(USE_NSS)
  // TODO(davidben): When https://bugzilla.mozilla.org/show_bug.cgi?id=564011
  // is fixed, we will no longer need the lock.
  base::Lock write_lock_;
#endif  // defined(USE_NSS)
};

// static
bool NSSInitSingleton::force_nodb_init_ = false;

base::LazyInstance<NSSInitSingleton,
                   base::LeakyLazyInstanceTraits<NSSInitSingleton> >
    g_nss_singleton(base::LINKER_INITIALIZED);

}  // namespace

#if defined(USE_NSS)
void EarlySetupForNSSInit() {
  FilePath database_dir = GetInitialConfigDirectory();
  if (!database_dir.empty())
    UseLocalCacheOfNSSDatabaseIfNFS(database_dir);
}
#endif

void EnsureNSPRInit() {
  g_nspr_singleton.Get();
}

void EnsureNSSInit() {
  // Initializing SSL causes us to do blocking IO.
  // Temporarily allow it until we fix
  //   http://code.google.com/p/chromium/issues/detail?id=59847
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  g_nss_singleton.Get();
}

void ForceNSSNoDBInit() {
  NSSInitSingleton::ForceNoDBInit();
}

void DisableNSSForkCheck() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar("NSS_STRICT_NOFORK", "DISABLED");
}

void LoadNSSLibraries() {
  // Some NSS libraries are linked dynamically so load them here.
#if defined(USE_NSS)
  // Try to search for multiple directories to load the libraries.
  std::vector<FilePath> paths;

  // Use relative path to Search PATH for the library files.
  paths.push_back(FilePath());

  // For Debian derivaties NSS libraries are located here.
  paths.push_back(FilePath("/usr/lib/nss"));

  // A list of library files to load.
  std::vector<std::string> libs;
  libs.push_back("libsoftokn3.so");
  libs.push_back("libfreebl3.so");

  // For each combination of library file and path, check for existence and
  // then load.
  size_t loaded = 0;
  for (size_t i = 0; i < libs.size(); ++i) {
    for (size_t j = 0; j < paths.size(); ++j) {
      FilePath path = paths[j].Append(libs[i]);
      base::NativeLibrary lib = base::LoadNativeLibrary(path, NULL);
      if (lib) {
        ++loaded;
        break;
      }
    }
  }

  if (loaded == libs.size()) {
    VLOG(3) << "NSS libraries loaded.";
  } else {
    LOG(WARNING) << "Failed to load NSS libraries.";
  }
#endif
}

bool CheckNSSVersion(const char* version) {
  return !!NSS_VersionCheck(version);
}

#if defined(USE_NSS)
bool OpenTestNSSDB(const FilePath& path, const char* description) {
  return g_nss_singleton.Get().OpenTestNSSDB(path, description);
}

void CloseTestNSSDB() {
  g_nss_singleton.Get().CloseTestNSSDB();
}

base::Lock* GetNSSWriteLock() {
  return g_nss_singleton.Get().write_lock();
}

AutoNSSWriteLock::AutoNSSWriteLock() : lock_(GetNSSWriteLock()) {
  // May be NULL if the lock is not needed in our version of NSS.
  if (lock_)
    lock_->Acquire();
}

AutoNSSWriteLock::~AutoNSSWriteLock() {
  if (lock_) {
    lock_->AssertAcquired();
    lock_->Release();
  }
}
#endif  // defined(USE_NSS)

#if defined(OS_CHROMEOS)
void OpenPersistentNSSDB() {
  g_nss_singleton.Get().OpenPersistentNSSDB();
}

TPMTokenInfoDelegate::TPMTokenInfoDelegate() {}
TPMTokenInfoDelegate::~TPMTokenInfoDelegate() {}

void EnableTPMTokenForNSS(TPMTokenInfoDelegate* info_delegate) {
  g_nss_singleton.Get().EnableTPMTokenForNSS(info_delegate);
}

void GetTPMTokenInfo(std::string* token_name, std::string* user_pin) {
  g_nss_singleton.Get().GetTPMTokenInfo(token_name, user_pin);
}

bool IsTPMTokenReady() {
  return g_nss_singleton.Get().IsTPMTokenReady();
}

#endif  // defined(OS_CHROMEOS)

// TODO(port): Implement this more simply.  We can convert by subtracting an
// offset (the difference between NSPR's and base::Time's epochs).
base::Time PRTimeToBaseTime(PRTime prtime) {
  PRExplodedTime prxtime;
  PR_ExplodeTime(prtime, PR_GMTParameters, &prxtime);

  base::Time::Exploded exploded;
  exploded.year         = prxtime.tm_year;
  exploded.month        = prxtime.tm_month + 1;
  exploded.day_of_week  = prxtime.tm_wday;
  exploded.day_of_month = prxtime.tm_mday;
  exploded.hour         = prxtime.tm_hour;
  exploded.minute       = prxtime.tm_min;
  exploded.second       = prxtime.tm_sec;
  exploded.millisecond  = prxtime.tm_usec / 1000;

  return base::Time::FromUTCExploded(exploded);
}

PK11SlotInfo* GetPublicNSSKeySlot() {
  return g_nss_singleton.Get().GetPublicNSSKeySlot();
}

PK11SlotInfo* GetPrivateNSSKeySlot() {
  return g_nss_singleton.Get().GetPrivateNSSKeySlot();
}

}  // namespace crypto
