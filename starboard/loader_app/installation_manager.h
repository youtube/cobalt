// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_LOADER_APP_INSTALLATION_MANAGER_H_
#define STARBOARD_LOADER_APP_INSTALLATION_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

// The installation is not successful.
#define IM_INSTALLATION_STATUS_NOT_SUCCESS 0

// The installation is successful.
#define IM_INSTALLATION_STATUS_SUCCESS 1

// An error occurred and the status of the installation is unknown.
#define IM_INSTALLATION_STATUS_ERROR -1

#define IM_SUCCESS 0
#define IM_ERROR -1

#define MAX_APP_KEY_LENGTH 1024

// The filename prefix for the Installation Manager store file.
#define IM_STORE_FILE_NAME_PREFIX "installation_store_"

// The filename suffix for the Installation Manager store file.
#define IM_STORE_FILE_NAME_SUFFIX ".pb"

// The max size in bytes of the store file.
#define IM_MAX_INSTALLATION_STORE_SIZE 1024 * 1024

// The max number a tries per installation before it
// is discarded if not successful.
#define IM_MAX_NUM_TRIES 3

// The Installation Manager API is thread safe and
// can be used from any thread. Most calls would
// trigger I/O operation unless the information is
// already cached in memory.

// Initializes the Installation Manager with the
// max number of installations and with an app specific key.
// If the store doesn't exist an initial store would be
// created. A subsequent call to |ImUninitialize| without
// corresponding |ImUninitialize| will fail.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImInitialize(int max_num_installations, const char* app_key);

// Retrieves the application key.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImGetAppKey(char* app_key, int app_key_length);

// Retrieves the max number of installation slots.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImGetMaxNumberInstallations();

// Uninitialize the Installation Manager.
void ImUninitialize();

// Resets the Installation Manager and clear all state.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImReset();

// Gets the status of the |installation_index| installation.
// Returns |IM_INSTALLATION_STATUS_SUCCESS| if the installation is successful.
// Returns |IM_INSTALLATION_STATUS_NOT_SUCCESS| if the installation is not
// successful. Returns |IM_INSTALLATION_STATUS_ERROR| on error.
int ImGetInstallationStatus(int installation_index);

// Returns the number of tries left for the |installation_index| installation.
// Returns |IM_ERROR| on error.
int ImGetInstallationNumTriesLeft(int installation_index);

// Decrements the number of tries left for installation |installation_index|.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImDecrementInstallationNumTries(int installation_index);

// Retrieves the current installation index.
// Returns |IM_ERROR| on error.
int ImGetCurrentInstallationIndex();

// Resets the slot for index |installation_index|.
// Returns |IM_ERROR| on error.
int ImResetInstallation(int installation_index);

// Selects a new installation index and prepares the installation
// slot for new installation use.
// Returns |IM_ERROR| on error.
int ImSelectNewInstallationIndex();

// Retrieves the absolute installation path for the installation
// |installation_index| in the buffer |path| with length |path_length|.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImGetInstallationPath(int installation_index, char* path, int path_length);

// Mark the installation |installation_index| as successful.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImMarkInstallationSuccessful(int installation_index);

// Rolls forward the installation slot requested to be used.
// The operation makes it current installation.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImRollForwardIfNeeded();

// Rolls forward to the slot at |installation_index|.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImRollForward(int installation_index);

// Revert to a previous successful installation.
// Returns the installation to which it was reverted.
// Returns |IM_ERROR| on error.
int ImRevertToSuccessfulInstallation();

// Request the installation at |installation_index| to be rolled
// forward next time the Loader App tries to load the app.
// Returns IM_SUCCESS on success and IM_ERROR on error.
int ImRequestRollForwardToInstallation(int installation_index);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_LOADER_APP_INSTALLATION_MANAGER_H_
