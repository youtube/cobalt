#!/bin/bash

# Script to download and extract RDK BSP

# --- Configuration ---
# The expected filename of the BSP
BSP_FILENAME="20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh"
# The URL for the BSP
BSP_URL="https://storage.googleapis.com/cobalt-static-storage-public/${BSP_FILENAME}" \

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
LOCAL_BSP_PATH="$DOWNLOAD_FOLDER/$BSP_FILENAME"

# 5. Fetch the BSP
log_info "Attempting to download BSP from '$BSP_URL' to '$LOCAL_BSP_PATH'..."
curl --silent "$BSP_URL" -o "$LOCAL_BSP_PATH"

# 7. Run the BSP shell script to extract files to RDK_HOME
log_info "Attempting to extract BSP '$LOCAL_BSP_PATH' to RDK_HOME ('$RDK_HOME')..."
sh "$LOCAL_BSP_PATH" -y -d "$RDK_HOME"

# Remove the temporarily downloaded RDK BSP installer
rm $LOCAL_BSP_PATH

log_info "RDK BSP download and extraction process finished."
exit 0
