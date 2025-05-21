#!/bin/bash

# // TODO: b/415859457 - Cobalt: Change the script once we have a publicly available BSP

# Script to download and extract RDK BSP(Binary Support Package)

# --- Configuration ---
# The Google Cloud Storage URL for the BSP
BSP_GS_URL="gs://temp_rdk_bsp_private/bsp_archive.tar.gz"
# The expected filename of the BSP
BSP_FILENAME="bsp_archive.tar.gz"

# --- Helper Functions for Logging ---
log_error() {
    echo "[ERROR] $(date +'%Y-%m-%d %H:%M:%S'): $1" >&2
}

log_info() {
    echo "[INFO] $(date +'%Y-%m-%d %H:%M:%S'): $1"
}

# --- Pre-flight Checks ---

# Assert: Check if RDK_HOME environment variable is set
if [ -z "$RDK_HOME" ]; then
    log_error "RDK_HOME environment variable is not set. Please set RDK_HOME to your RDK build directory and try again."
    exit 1
fi

# Check for script arguments: exactly one argument for the download folder
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <target_download_folder>"
    log_error "Invalid number of arguments. Please provide exactly one argument for the target download folder."
    exit 1
fi

DOWNLOAD_FOLDER="$1"
log_info "User-specified download folder: $DOWNLOAD_FOLDER"

# Create the download folder if it doesn't exist
if [ ! -d "$DOWNLOAD_FOLDER" ]; then
    log_info "Download folder '$DOWNLOAD_FOLDER' does not exist. Attempting to create it..."
    mkdir -p "$DOWNLOAD_FOLDER"
    if [ $? -ne 0 ]; then
        log_error "Failed to create download folder '$DOWNLOAD_FOLDER'. Please check permissions and path."
        exit 1
    else
        log_info "Successfully created download folder '$DOWNLOAD_FOLDER'."
    fi
else
    log_info "Download folder '$DOWNLOAD_FOLDER' already exists."
fi

# Define the full path for the downloaded BSP file
LOCAL_BSP_ARCHIVE="$DOWNLOAD_FOLDER/$BSP_FILENAME"

# 5. Copy the BSP using gcloud
log_info "Attempting to download BSP from '$BSP_GS_URL' to '$LOCAL_BSP_ARCHIVE'..."
# A pre-requisite to running this command is getting access to gcs bucket
# if you dont have access, ask cobalt team for access to RDK BSP found here:
# http://dr/drive/folders/1P9CkVlriQ1GYu0mC4nufBcxcn5kwpVza
gcloud storage cp "$BSP_GS_URL" "$LOCAL_BSP_ARCHIVE"

# 7. Run the BSP shell script to extract files to RDK_HOME
log_info "Attempting to extract BSP '$LOCAL_BSP_ARCHIVE' to RDK_HOME ('$RDK_HOME')..."
tar -xvf "$LOCAL_BSP_ARCHIVE" -C "$RDK_HOME"

# Remove the temporarily downloaded RDK BSP installer
rm $LOCAL_BSP_ARCHIVE

log_info "RDK BSP download and extraction process finished."
exit 0
