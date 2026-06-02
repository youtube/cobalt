#!/bin/bash
# Shared helpers for Cobalt build/deploy/run scripts

# Default target IP/hostname based on the local machine hostname
case "$(hostname)" in
    denis-dell) DEFAULT_TARGET="192.168.15.112" ;;
    oasis)      DEFAULT_TARGET="yukinet.ddns.net" ;;
    *)          DEFAULT_TARGET="192.168.15.112" ;;
esac

# Common configuration variables shared across scripts
DEFAULT_BUILD_CONFIG="${DEFAULT_BUILD_CONFIG:-devel}"
BUILD_CONFIG="${CONFIG:-${BUILD_CONFIG:-$DEFAULT_BUILD_CONFIG}}"
BRANCH="${BRANCH:-main}"
COBALT_BUILD_DIR="${COBALT_BUILD_DIR:-$HOME/RDK/cobalt-build}"
DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-$HOME/dpot_tools}"

# Optional Cobalt test executable targets for ninja, runtime_deps deploy, and
# direct on-target execution via `./<test_name>`.
# COBALT_TEST_EXECUTABLE_STEMS=(
#     nplb
#     base_unittests
#     blink_common_unittests
#     boringssl_crypto_tests
#     boringssl_pki_tests
#     boringssl_ssl_tests
#     cc_unittests
#     compositor_unittests
#     google_apis_unittests
#     opus_tests
#     ozone_unittests
#     sql_unittests
#     storage_unittests
#     test_opus_decode
#     test_opus_encode
#     test_opus_padding
#     url_unittests
#     viz_unittests
#     views_examples_unittests
#     zlib_unittests
# )

COBALT_TEST_EXECUTABLE_STEMS=(
    nplb_loader
)

print_error()   { echo -e "\033[1;31mError: $1\033[0m" >&2; }
print_success() { echo -e "\033[1;32m$1\033[0m"; }
print_warning() { echo -e "\033[1;33mWarning: $1\033[0m"; }
print_info()    { echo -e "\033[1;36m$1\033[0m"; }
print_cmd()     { echo -e "\033[1;35m+ $1\033[0m"; }

# Injected into container build script heredocs via $CONTAINER_LOG_FUNCTIONS.
# Single-quoted so $* and $(date) stay literal here and expand inside the container.
CONTAINER_LOG_FUNCTIONS='
clog()     { echo -e "\033[1;36m[container $(date "+%T")] $*\033[0m"; }
clog_cmd() { echo -e "\033[1;35m[container $(date "+%T")] + $*\033[0m"; }
'

terminate_cobalt() {
    local cobalt_dir="${1:-/data/out_cobalt}"
    print_info "Terminating Cobalt session..."
    if ! ssh_run_nonfatal "cd \"$cobalt_dir\" && killall -9 elf_loader_sandbox cobalt_loader loader_app 2>/dev/null || true"; then
        print_error "Failed to terminate Cobalt on $TARGET"
    fi
}

# OpenSSH 8.8+ disables ssh-rsa by default; RDK/Dropbear often still needs it.
# BatchMode=yes avoids blocking on a password prompt when key auth fails (CI/automation).
# Override before sourcing: COBALT_SSH_EXTRA_OPTS='-o PreferredAuthentications=password' etc.
COBALT_SSH_EXTRA_OPTS="${COBALT_SSH_EXTRA_OPTS:--o BatchMode=yes -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa}"

# SSH helpers (caller sets SSH_OPTS, TARGET; TIMEOUT_CMD optional for ssh_run_with_timeout)
ssh_capture() {
    local cmd="$1"
    local context="${2:-}"
    local output
    if ! output=$(ssh $SSH_OPTS $COBALT_SSH_EXTRA_OPTS "root@$TARGET" "$cmd" 2>/dev/null); then
        print_error "SSH connection failed to $TARGET${context:+ - $context}"
        exit 1
    fi
    echo "$output"
}

ssh_run() {
    local cmd="$1"
    local context="${2:-}"
    print_info "Running command on $TARGET: $cmd"
    if ! ssh $SSH_OPTS $COBALT_SSH_EXTRA_OPTS "root@$TARGET" "$cmd" 2>/dev/null; then
        print_error "SSH connection failed to $TARGET${context:+ - $context}"
        exit 1
    fi
}

ssh_run_nonfatal() {
    local cmd="$1"
    if ! ssh $SSH_OPTS $COBALT_SSH_EXTRA_OPTS "root@$TARGET" "$cmd" 2>/dev/null; then
        return 1
    fi
}

# Runs cmd over SSH; if TIMEOUT_CMD is set, wraps with it (exit 124 on timeout).
# Third arg "keep_stderr" keeps stderr; otherwise stderr is discarded.
ssh_run_with_timeout() {
    local cmd="$1"
    local stderr_mode="${3:-suppress}"
    local stderr_redirect="2>/dev/null"
    [[ "$stderr_mode" == "keep_stderr" ]] && stderr_redirect=""
    if [[ -n "${TIMEOUT_CMD:-}" ]]; then
        $TIMEOUT_CMD ssh $SSH_OPTS $COBALT_SSH_EXTRA_OPTS "root@$TARGET" "$cmd" $stderr_redirect
    else
        ssh $SSH_OPTS $COBALT_SSH_EXTRA_OPTS "root@$TARGET" "$cmd" $stderr_redirect
    fi
}
