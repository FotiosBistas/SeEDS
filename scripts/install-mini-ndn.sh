#!/usr/bin/env bash

set -euo pipefail

#####################################################################
# Paths
#####################################################################

SEEDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MININDN_DIR="$SEEDS_ROOT/external/mini-ndn"
NDN_CXX_DIR="$MININDN_DIR/dl/ndn-cxx"
MININET_DIR="$MININDN_DIR/dl/mininet"
OPENFLOW_DIR="$MININDN_DIR/dl/openflow"

NDN_CXX_PATCH="$SEEDS_ROOT/cmake/patches/ndn-cxx-0.9.0-gcc16.patch"
OPENFLOW_PATCH="$SEEDS_ROOT/cmake/patches/openflow-gcc16.patch"
MININET_HOOK_PATCH="$SEEDS_ROOT/cmake/patches/mininet-openflow-patch-hook.patch"


#####################################################################
# Banner
#####################################################################

echo
echo "============================================================"
echo " Mini-NDN setup for SeEDS"
echo "============================================================"
echo


#####################################################################
# Initialize Mini-NDN Git submodule
#####################################################################

echo "==> Initializing Mini-NDN submodule"

git -C "$SEEDS_ROOT" submodule update \
    --init \
    --recursive \
    external/mini-ndn


#####################################################################
# Verify required SeEDS patch files
#####################################################################

echo
echo "==> Checking SeEDS compatibility patches"

if [[ ! -f "$NDN_CXX_PATCH" ]]; then
    echo "ERROR: ndn-cxx compatibility patch does not exist:"
    echo "       $NDN_CXX_PATCH"
    exit 1
fi

if [[ ! -f "$OPENFLOW_PATCH" ]]; then
    echo "ERROR: OpenFlow compatibility patch does not exist:"
    echo "       $OPENFLOW_PATCH"
    exit 1
fi

if [[ ! -f "$MININET_HOOK_PATCH" ]]; then
    echo "ERROR: Mininet OpenFlow hook patch does not exist:"
    echo "       $MININET_HOOK_PATCH"
    exit 1
fi

echo "==> SeEDS patch files found"


#####################################################################
# Detect Linux distribution
#####################################################################

echo

if [[ ! -r /etc/os-release ]]; then
    echo "ERROR: Cannot determine Linux distribution"
    exit 1
fi

# Provides ID, VERSION_ID, etc.
. /etc/os-release

echo "==> Detected distribution: $ID ${VERSION_ID:-unknown}"


#####################################################################
# Distribution-specific compatibility dependencies
#####################################################################

case "$ID" in

    fedora)
        echo "==> Applying Fedora compatibility setup"

        #
        # Mininet's installer expects lsb_release. On modern Fedora,
        # the old redhat-lsb-core package is no longer available.
        #
        if ! command -v lsb_release >/dev/null 2>&1; then
            echo "==> Installing Fedora dependency: lsb_release"
            sudo dnf install -y lsb_release
        fi

        #
        # Mininet uses the traditional 'patch' utility while building
        # its OpenFlow dependency.
        #
        if ! command -v patch >/dev/null 2>&1; then
            echo "==> Installing Fedora dependency: patch"
            sudo dnf install -y patch
        fi

        if ! command -v iproute-tc >/dev/null 2>&1; then
            echo "==> Installing Fedora dependency: patch"
            sudo dnf install -y iproute-tc
        fi
        ;;


    ubuntu|debian)
        echo "==> Applying Debian/Ubuntu compatibility setup"

        PACKAGES=()

        if ! command -v patch >/dev/null 2>&1; then
            PACKAGES+=(patch)
        fi

        if ! command -v lsb_release >/dev/null 2>&1; then
            PACKAGES+=(lsb-release)
        fi

        if ! command -v iproute-tc >/dev/null 2>&1; then
            PACKAGES+=(iproute-tc)
        fi

        if ((${#PACKAGES[@]} > 0)); then
            echo "==> Installing dependencies: ${PACKAGES[*]}"
            sudo apt-get update
            sudo apt-get install -y "${PACKAGES[@]}"
        fi
        ;;


    *)
        echo "WARNING: No SeEDS-specific compatibility setup exists"
        echo "         for distribution '$ID'."
        echo
        echo "Mini-NDN's own installer will attempt to install its"
        echo "normal dependencies."
        ;;
esac


#####################################################################
# Download Mini-NDN dependency source trees
#
# --dl-only downloads/checks out the dependencies but does not build
# them yet. This gives SeEDS an opportunity to apply compatibility
# patches before compilation.
#####################################################################

echo
echo "==> Downloading Mini-NDN dependencies"

cd "$MININDN_DIR"

./install.sh \
    --source \
    --release=2024-08 \
    --no-wifi \
    --dl-only \
    -y


#####################################################################
# Verify downloaded source trees
#####################################################################

echo
echo "==> Verifying downloaded source trees"

if [[ ! -d "$NDN_CXX_DIR/.git" ]]; then
    echo "ERROR: ndn-cxx source was not downloaded:"
    echo "       $NDN_CXX_DIR"
    exit 1
fi

if [[ ! -d "$MININET_DIR/.git" ]]; then
    echo "ERROR: Mininet source was not downloaded:"
    echo "       $MININET_DIR"
    exit 1
fi

echo "==> Downloaded source trees verified"


#####################################################################
# Apply ndn-cxx GCC 16 compatibility patch
#
# ndn-cxx 0.9.0 uses uint64_t in packet-base.hpp without explicitly
# including <cstdint>. GCC 16 exposes this missing include.
#####################################################################

echo
echo "==> Checking ndn-cxx GCC 16 compatibility patch"

if git -C "$NDN_CXX_DIR" apply \
        --reverse \
        --check \
        "$NDN_CXX_PATCH" \
        2>/dev/null; then

    echo "==> ndn-cxx patch already applied"

elif git -C "$NDN_CXX_DIR" apply \
        --check \
        "$NDN_CXX_PATCH"; then

    echo "==> Applying ndn-cxx compatibility patch"

    git -C "$NDN_CXX_DIR" apply \
        "$NDN_CXX_PATCH"

    echo "==> ndn-cxx patch applied"

else
    echo
    echo "ERROR: ndn-cxx compatibility patch cannot be applied:"
    echo "       $NDN_CXX_PATCH"
    echo
    echo "The downloaded ndn-cxx source may have changed."
    exit 1
fi


#####################################################################
# Verify ndn-cxx patch
#####################################################################

echo
echo "==> Verifying ndn-cxx compatibility patch"

if ! grep -q \
    '#include <cstdint>' \
    "$NDN_CXX_DIR/ndn-cxx/detail/packet-base.hpp"; then

    echo "ERROR: <cstdint> was not found in packet-base.hpp"
    exit 1
fi

echo "==> ndn-cxx GCC 16 patch verified"


#####################################################################
# Patch Mininet's installer
#
# OpenFlow is not downloaded by Mini-NDN's --dl-only phase.
# Mininet clones OpenFlow later while Mininet itself is being
# installed.
#
# Therefore SeEDS cannot patch OpenFlow here directly.
#
# Instead we patch Mininet's util/install.sh so that immediately
# after Mininet clones OpenFlow, it applies:
#
#     cmake/patches/openflow-gcc16.patch
#
#####################################################################

echo
echo "==> Checking Mininet OpenFlow patch hook"

if git -C "$MININET_DIR" apply \
        --reverse \
        --check \
        "$MININET_HOOK_PATCH" \
        2>/dev/null; then

    echo "==> Mininet OpenFlow patch hook already applied"

elif git -C "$MININET_DIR" apply \
        --check \
        "$MININET_HOOK_PATCH"; then

    echo "==> Applying Mininet OpenFlow patch hook"

    git -C "$MININET_DIR" apply \
        "$MININET_HOOK_PATCH"

    echo "==> Mininet OpenFlow patch hook applied"

else
    echo
    echo "ERROR: Mininet OpenFlow patch hook cannot be applied:"
    echo "       $MININET_HOOK_PATCH"
    echo
    echo "The downloaded Mininet source may have changed."
    exit 1
fi


#####################################################################
# Verify Mininet hook
#####################################################################

echo
echo "==> Verifying Mininet OpenFlow patch hook"

if ! grep -q \
    'SEEDS_OPENFLOW_PATCH' \
    "$MININET_DIR/util/install.sh"; then

    echo "ERROR: Mininet installer does not contain the"
    echo "       SEEDS_OPENFLOW_PATCH hook."
    exit 1
fi

echo "==> Mininet OpenFlow patch hook verified"


#####################################################################
# Remove stale OpenFlow checkout
#
# OpenFlow is generated during the Mininet installation. A previous
# failed installation may have left a partially built or already
# modified checkout behind.
#
# Removing it ensures Mininet creates a clean OpenFlow checkout and
# that our compatibility patch is applied exactly once.
#####################################################################

echo
echo "==> Removing stale OpenFlow checkout"

rm -rf "$OPENFLOW_DIR"


#####################################################################
# Export OpenFlow compatibility patch
#
# Mininet's patched util/install.sh runs as a child process of the
# Mini-NDN installer. The path must therefore be exported into the
# environment.
#####################################################################

export SEEDS_OPENFLOW_PATCH="$OPENFLOW_PATCH"

echo
echo "==> OpenFlow compatibility patch:"
echo "    $SEEDS_OPENFLOW_PATCH"


#####################################################################
# Build and install Mini-NDN
#
# At this stage:
#
#   1. ndn-cxx has already been patched.
#   2. Mininet's installer contains our OpenFlow hook.
#   3. SEEDS_OPENFLOW_PATCH is exported.
#
# During Mininet installation the expected sequence is:
#
#   git clone OpenFlow
#       -> apply openflow-gcc16.patch
#       -> apply Mininet's normal OpenFlow patches
#       -> configure
#       -> build
#       -> install
#
#####################################################################

echo
echo "==> Building and installing Mini-NDN"

cd "$MININDN_DIR"

./install.sh \
    --source \
    --release=2024-08 \
    --no-wifi \
    -y


#####################################################################
# Final verification
#####################################################################

echo
echo "==> Verifying Mini-NDN installation"

if command -v mn >/dev/null 2>&1; then
    echo "==> Mininet installed: $(command -v mn)"
else
    echo "WARNING: 'mn' was not found in PATH after installation."
fi

if command -v nfd >/dev/null 2>&1; then
    echo "==> NFD installed: $(command -v nfd)"
else
    echo "WARNING: 'nfd' was not found in PATH after installation."
fi


#####################################################################
# Completed
#####################################################################

echo
echo "============================================================"
echo " Mini-NDN installation completed"
echo "============================================================"
echo
