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
MININET_UBUNTU_PATCH="$SEEDS_ROOT/cmake/patches/mininet-ubuntu-2404.patch"
MININDN_SEEDS_BOOST_PATCH="$SEEDS_ROOT/cmake/patches/mini-ndn-seeds-boost.patch"


#####################################################################
# Existing SeEDS build paths
#
# Mini-NDN remains completely separate from SeEDS.
#
# We only expose compatible artifacts that already exist in build/
# so Mini-NDN can reuse them through --use-existing.
#####################################################################

SEEDS_BUILD_DIR="$SEEDS_ROOT/build"

SEEDS_BOOST_DIR="$SEEDS_BUILD_DIR/boost-install"
SEEDS_SQLITE_DIR="$SEEDS_BUILD_DIR/sqlite-install"
SEEDS_NDN_CXX_DIR="$SEEDS_BUILD_DIR/ndn-cxx/install"
SEEDS_NDN_TOOLS_DIR="$SEEDS_BUILD_DIR/ndn-tools/install"
SEEDS_NFD_DIR="$SEEDS_BUILD_DIR/NFD/install"

REUSE_NFD=0
REUSE_BOOST=0
REUSE_SQLITE=0
REUSE_NDN_CXX=0
REUSE_NDN_TOOLS=0


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
# Detect Linux distribution
#####################################################################

echo

if [[ -r /etc/os-release ]]; then
    . /etc/os-release
else
    echo "ERROR: Cannot determine Linux distribution"
    exit 1
fi

echo "==> Detected distribution: $ID ${VERSION_ID:-unknown}"


#####################################################################
# Reuse compatible SeEDS build artifacts when available
#
# Nothing is copied from build/.
#
# Mini-NDN only sees the already-built artifacts through normal
# environment variables and its --use-existing mechanism.
#####################################################################

echo
echo "==> Checking reusable SeEDS build artifacts"

#
# Do not allow shell state inherited from the caller to influence
# dependency detection. The script establishes all paths it needs
# explicitly below.
#
unset CC CXX
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
unset CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH
unset LIBRARY_PATH LD_LIBRARY_PATH
unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
unset CMAKE_PREFIX_PATH
unset BOOST_ROOT BOOSTROOT BOOST_INCLUDEDIR BOOST_LIBRARYDIR

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"


#####################################################################
# Boost
#####################################################################

if [[ -f "$SEEDS_BOOST_DIR/include/boost/version.hpp" ]] &&
   [[ -d "$SEEDS_BOOST_DIR/lib" ]]; then

    REUSE_BOOST=1

    echo "==> Found SeEDS Boost:"
    echo "    $SEEDS_BOOST_DIR"

    export CPLUS_INCLUDE_PATH="\
$SEEDS_BOOST_DIR/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"

    export LIBRARY_PATH="\
$SEEDS_BOOST_DIR/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"

    export LD_LIBRARY_PATH="\
$SEEDS_BOOST_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

else
    echo "==> SeEDS Boost not available"
fi


#####################################################################
# SQLite
#####################################################################

if [[ -f "$SEEDS_SQLITE_DIR/include/sqlite3.h" ]] &&
   [[ -d "$SEEDS_SQLITE_DIR/lib" ]]; then

    REUSE_SQLITE=1

    echo "==> Found SeEDS SQLite:"
    echo "    $SEEDS_SQLITE_DIR"

    export CPLUS_INCLUDE_PATH="\
$SEEDS_SQLITE_DIR/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"

    export LIBRARY_PATH="\
$SEEDS_SQLITE_DIR/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"

    export LD_LIBRARY_PATH="\
$SEEDS_SQLITE_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

    if [[ -d "$SEEDS_SQLITE_DIR/lib/pkgconfig" ]]; then
        export PKG_CONFIG_PATH="\
$SEEDS_SQLITE_DIR/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi

else
    echo "==> SeEDS SQLite not available"
fi


#####################################################################
# ndn-cxx
#
# Mini-NDN detects an existing ndn-cxx using:
#
#     pkg-config libndn-cxx
#
# Probe the SeEDS pkg-config metadata without globally exposing it
# first. Only export it when the expected version is present.
#####################################################################

if [[ -f "$SEEDS_NDN_CXX_DIR/lib/pkgconfig/libndn-cxx.pc" ]] &&
   [[ -f "$SEEDS_NDN_CXX_DIR/lib/libndn-cxx.so" ]]; then

    NDN_CXX_VERSION="$(
        PKG_CONFIG_PATH="\
$SEEDS_NDN_CXX_DIR/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
        pkg-config --modversion libndn-cxx 2>/dev/null || true
    )"

    if [[ "$NDN_CXX_VERSION" == "0.9.0" ]]; then

        REUSE_NDN_CXX=1

        echo "==> Reusing SeEDS ndn-cxx $NDN_CXX_VERSION:"
        echo "    $SEEDS_NDN_CXX_DIR"

        export PKG_CONFIG_PATH="\
$SEEDS_NDN_CXX_DIR/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

        export CPLUS_INCLUDE_PATH="\
$SEEDS_NDN_CXX_DIR/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"

        export LIBRARY_PATH="\
$SEEDS_NDN_CXX_DIR/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"

        export LD_LIBRARY_PATH="\
$SEEDS_NDN_CXX_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

    else

        echo "==> SeEDS ndn-cxx is not the expected version"
        echo "    Found:    ${NDN_CXX_VERSION:-unknown}"
        echo "    Expected: 0.9.0"
        echo "    Mini-NDN will build its own ndn-cxx"
    fi

else
    echo "==> SeEDS ndn-cxx not available"
    echo "    Mini-NDN will build its own ndn-cxx"
fi


#####################################################################
# NFD
#
# Reuse the NFD already built by SeEDS when all Mini-NDN-required
# helper commands and the sample configuration are available.
#####################################################################

if ((REUSE_NDN_CXX)) &&
   [[ -x "$SEEDS_NFD_DIR/bin/nfd" ]] &&
   [[ -x "$SEEDS_NFD_DIR/bin/nfdc" ]] &&
   [[ -x "$SEEDS_NFD_DIR/bin/nfd-start" ]] &&
   [[ -x "$SEEDS_NFD_DIR/bin/nfd-stop" ]] &&
   [[ -f "$SEEDS_NFD_DIR/etc/ndn/nfd.conf.sample" ]]; then

    REUSE_NFD=1

    echo "==> Reusing SeEDS NFD:"
    echo "    $SEEDS_NFD_DIR"

    export PATH="$SEEDS_NFD_DIR/bin:$PATH"

else
    echo "==> SeEDS NFD not reusable"
    echo "    Mini-NDN will build its own NFD"
fi


#####################################################################
# ndn-tools
#
# Mini-NDN detects these using:
#
#     command -v ndnping
#     command -v ndncatchunks
#
# Only expose them if we are also reusing the SeEDS ndn-cxx that
# these binaries were built against.
#####################################################################

if ((REUSE_NDN_CXX)) &&
   [[ -x "$SEEDS_NDN_TOOLS_DIR/bin/ndnping" ]] &&
   [[ -x "$SEEDS_NDN_TOOLS_DIR/bin/ndncatchunks" ]]; then

    REUSE_NDN_TOOLS=1

    echo "==> Reusing SeEDS ndn-tools:"
    echo "    $SEEDS_NDN_TOOLS_DIR"

    export PATH="\
$SEEDS_NDN_TOOLS_DIR/bin:$PATH"

else
    echo "==> SeEDS ndn-tools not reusable"
    echo "    Mini-NDN will build its own ndn-tools"
fi


#####################################################################
# NLSR Boost configuration
#
# When Mini-NDN reuses SeEDS ndn-cxx, NLSR must use the same Boost
# installation that ndn-cxx was built against. Generic LDFLAGS and
# include-path variables are not enough because NLSR's Waf configure
# step performs its own Boost discovery.
#
# mini-ndn-seeds-boost.patch makes Mini-NDN pass this value to the
# NLSR Waf configure command.
#####################################################################

if ((REUSE_NDN_CXX)) && ((REUSE_BOOST)); then

    export SEEDS_NLSR_WAFOPTS="\
--boost-includes=$SEEDS_BOOST_DIR/include \
--boost-libs=$SEEDS_BOOST_DIR/lib"

    echo
    echo "==> NLSR will use SeEDS Boost:"
    echo "    includes: $SEEDS_BOOST_DIR/include"
    echo "    libs:     $SEEDS_BOOST_DIR/lib"

else
    unset SEEDS_NLSR_WAFOPTS
fi


#####################################################################
# Runtime/linker paths for Mini-NDN-built components
#
# PSync, NLSR, etc. may be built by Mini-NDN against the SeEDS-local
# ndn-cxx. Add RPATH entries so those programs can locate the reused
# libraries after installation.
#####################################################################

SEEDS_LDFLAGS=""

if ((REUSE_NDN_CXX)); then
    SEEDS_LDFLAGS+=" -L$SEEDS_NDN_CXX_DIR/lib"
    SEEDS_LDFLAGS+=" -Wl,-rpath,$SEEDS_NDN_CXX_DIR/lib"
fi

if ((REUSE_BOOST)); then
    SEEDS_LDFLAGS+=" -L$SEEDS_BOOST_DIR/lib"
    SEEDS_LDFLAGS+=" -Wl,-rpath,$SEEDS_BOOST_DIR/lib"
fi

if ((REUSE_SQLITE)); then
    SEEDS_LDFLAGS+=" -L$SEEDS_SQLITE_DIR/lib"
    SEEDS_LDFLAGS+=" -Wl,-rpath,$SEEDS_SQLITE_DIR/lib"
fi

if [[ -n "$SEEDS_LDFLAGS" ]]; then
    export LDFLAGS="${LDFLAGS:-}$SEEDS_LDFLAGS"
fi


#####################################################################
# Reuse summary
#####################################################################

echo
echo "==> Reuse summary"

if ((REUSE_BOOST)); then
    echo "    Boost:     SeEDS/build"
else
    echo "    Boost:     Mini-NDN/system"
fi

if ((REUSE_SQLITE)); then
    echo "    SQLite:    SeEDS/build"
else
    echo "    SQLite:    Mini-NDN/system"
fi

if ((REUSE_NDN_CXX)); then
    echo "    ndn-cxx:   SeEDS/build"
else
    echo "    ndn-cxx:   Mini-NDN"
fi

if ((REUSE_NFD)); then
    echo "    NFD:       SeEDS/build"
else
    echo "    NFD:       Mini-NDN"
fi

if ((REUSE_NDN_TOOLS)); then
    echo "    ndn-tools: SeEDS/build"
else
    echo "    ndn-tools: Mini-NDN"
fi


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

if [[ ! -f "$MININET_UBUNTU_PATCH" ]]; then
    echo "ERROR: Mininet Ubuntu compatibility patch does not exist:"
    echo "       $MININET_UBUNTU_PATCH"
    exit 1
fi

if [[ ! -f "$MININDN_SEEDS_BOOST_PATCH" ]]; then
    echo "ERROR: Mini-NDN SeEDS Boost patch does not exist:"
    echo "       $MININDN_SEEDS_BOOST_PATCH"
    exit 1
fi

echo "==> SeEDS patch files found"


#####################################################################
# Distribution-specific compatibility dependencies
#####################################################################

case "$ID" in

    fedora)
        echo "==> Applying Fedora compatibility setup"

        if ! command -v lsb_release >/dev/null 2>&1; then
            echo "==> Installing Fedora compatibility dependency: lsb_release"
            sudo dnf install -y lsb_release
        fi

        if ! command -v patch >/dev/null 2>&1; then
            echo "==> Installing compatibility dependency: patch"
            sudo dnf install -y patch
        fi

        if ! command -v tc >/dev/null 2>&1; then
            echo "==> Installing Fedora compatibility dependency: iproute-tc"
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

        if ! command -v tc >/dev/null 2>&1; then
            PACKAGES+=(iproute2)
        fi

        if ((${#PACKAGES[@]} > 0)); then
            echo "==> Installing dependencies: ${PACKAGES[*]}"
            sudo apt-get update
            sudo apt-get install -y "${PACKAGES[@]}"
        fi
        ;;


    *)
        echo "WARNING: No SeEDS-specific Mini-NDN compatibility fixes defined."
        ;;
esac


#####################################################################
# Patch Mini-NDN so NLSR can receive explicit Boost Waf options
#####################################################################

echo
echo "==> Checking Mini-NDN SeEDS Boost patch"

if git -C "$MININDN_DIR" apply \
        --reverse \
        --check \
        "$MININDN_SEEDS_BOOST_PATCH" \
        2>/dev/null; then

    echo "==> Mini-NDN SeEDS Boost patch already applied"

elif git -C "$MININDN_DIR" apply \
        --check \
        "$MININDN_SEEDS_BOOST_PATCH"; then

    echo "==> Applying Mini-NDN SeEDS Boost patch"

    git -C "$MININDN_DIR" apply \
        "$MININDN_SEEDS_BOOST_PATCH"

    echo "==> Mini-NDN SeEDS Boost patch applied"

else

    echo
    echo "ERROR: Mini-NDN SeEDS Boost patch cannot be applied:"
    echo "       $MININDN_SEEDS_BOOST_PATCH"
    echo
    echo "The Mini-NDN source may have changed."
    exit 1
fi


#####################################################################
# pkg-config paths for Mini-NDN-installed dependencies
#
# Dependencies such as PSync are installed by Mini-NDN into
# /usr/local. On Fedora, Waf normally installs pkg-config metadata
# under /usr/local/lib64/pkgconfig.
#####################################################################

export PKG_CONFIG_PATH="\
/usr/local/lib64/pkgconfig:\
/usr/local/lib/pkgconfig\
${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"


#####################################################################
# Download Mini-NDN dependency source trees
#
# --use-existing causes Mini-NDN to skip compatible dependencies
# already visible through PATH/pkg-config.
#####################################################################

echo
echo "==> Downloading Mini-NDN dependencies"

cd "$MININDN_DIR"

./install.sh \
    --source \
    --release=2024-08 \
    --no-wifi \
    --use-existing \
    --dl-only \
    -y


#####################################################################
# Verify downloaded source trees
#####################################################################

echo
echo "==> Verifying downloaded source trees"

if [[ -d "$NDN_CXX_DIR/.git" ]]; then
    echo "==> Mini-NDN downloaded its own ndn-cxx"
elif ((REUSE_NDN_CXX)); then
    echo "==> Mini-NDN is reusing SeEDS ndn-cxx"
else
    echo "ERROR: No usable ndn-cxx was found or downloaded"
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
#####################################################################

if [[ -d "$NDN_CXX_DIR/.git" ]]; then

    echo
    echo "==> Checking ndn-cxx GCC 16 compatibility patch"

    if git -C "$NDN_CXX_DIR" apply \
            --reverse \
            --check \
            "$NDN_CXX_PATCH" \
            2>/dev/null; then

        echo "==> Patch already applied"

    elif git -C "$NDN_CXX_DIR" apply \
            --check \
            "$NDN_CXX_PATCH"; then

        echo "==> Applying patch"

        git -C "$NDN_CXX_DIR" apply \
            "$NDN_CXX_PATCH"

    else

        echo
        echo "ERROR: Patch cannot be applied:"
        echo "       $NDN_CXX_PATCH"
        echo
        echo "The downloaded ndn-cxx version may have changed."
        exit 1
    fi

    echo
    echo "==> Verifying patch"

    if ! grep -q '#include <cstdint>' \
        "$NDN_CXX_DIR/ndn-cxx/detail/packet-base.hpp"; then

        echo "ERROR: <cstdint> was not found after patching"
        exit 1
    fi

    echo "==> GCC 16 patch verified"

else

    echo
    echo "==> Skipping Mini-NDN ndn-cxx patch"
    echo "    Using SeEDS-built ndn-cxx instead"
fi


#####################################################################
# Apply Ubuntu 24.04 Mininet compatibility patch
#####################################################################

if [[ "$ID" == "ubuntu" ]] &&
   dpkg --compare-versions "${VERSION_ID:-0}" ge "24.04"; then

    echo
    echo "==> Checking Ubuntu 24.04 Mininet compatibility patch"

    if git -C "$MININET_DIR" apply \
            --reverse \
            --check \
            "$MININET_UBUNTU_PATCH" \
            2>/dev/null; then

        echo "==> Ubuntu 24.04 Mininet compatibility patch already applied"

    elif git -C "$MININET_DIR" apply \
            --check \
            "$MININET_UBUNTU_PATCH"; then

        echo "==> Applying Ubuntu 24.04 Mininet compatibility patch"

        git -C "$MININET_DIR" apply \
            "$MININET_UBUNTU_PATCH"

        echo "==> Ubuntu 24.04 Mininet compatibility patch applied"

    else

        echo
        echo "ERROR: Ubuntu 24.04 Mininet compatibility patch cannot be applied:"
        echo "       $MININET_UBUNTU_PATCH"
        echo
        echo "The downloaded Mininet source may have changed."
        exit 1
    fi

    if ! grep -q 'pep8=pycodestyle' \
        "$MININET_DIR/util/install.sh"; then

        echo "ERROR: Mininet Ubuntu compatibility patch verification failed."
        echo "       pep8=pycodestyle was not found in:"
        echo "       $MININET_DIR/util/install.sh"
        exit 1
    fi

    echo "==> Ubuntu 24.04 Mininet compatibility patch verified"
fi


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
#####################################################################

echo
echo "==> Removing stale OpenFlow checkout"

if [[ -e "$OPENFLOW_DIR" ]]; then
    sudo rm -rf -- "$OPENFLOW_DIR"
fi


#####################################################################
# Export OpenFlow compatibility patch
#####################################################################

export SEEDS_OPENFLOW_PATCH="$OPENFLOW_PATCH"

echo
echo "==> OpenFlow compatibility patch:"
echo "    $SEEDS_OPENFLOW_PATCH"


#####################################################################
# Register reused SeEDS libraries with the dynamic linker
#
# Reused SeEDS binaries such as nfdc do not necessarily contain a
# RUNPATH for SeEDS-local Boost/ndn-cxx/SQLite libraries. Register
# those directories system-wide for Mini-NDN runtime use.
#####################################################################

SEEDS_LD_CONF="/etc/ld.so.conf.d/seeds-mini-ndn.conf"
SEEDS_LD_PATHS=()

if ((REUSE_NDN_CXX)); then
    SEEDS_LD_PATHS+=("$SEEDS_NDN_CXX_DIR/lib")
fi

if ((REUSE_BOOST)); then
    SEEDS_LD_PATHS+=("$SEEDS_BOOST_DIR/lib")
fi

if ((REUSE_SQLITE)); then
    SEEDS_LD_PATHS+=("$SEEDS_SQLITE_DIR/lib")
fi

if ((${#SEEDS_LD_PATHS[@]} > 0)); then
    echo
    echo "==> Registering reused SeEDS library paths"

    printf '%s\n' "${SEEDS_LD_PATHS[@]}" \
        | sudo tee "$SEEDS_LD_CONF" >/dev/null

    sudo ldconfig
else
    if [[ -f "$SEEDS_LD_CONF" ]]; then
        sudo rm -f "$SEEDS_LD_CONF"
        sudo ldconfig
    fi
fi


#####################################################################
# Build and install Mini-NDN
#
# Mini-NDN remains responsible for all dependencies not already
# available from the SeEDS build.
#####################################################################

echo
echo "==> Building and installing Mini-NDN"

cd "$MININDN_DIR"

./install.sh \
    --source \
    --release=2024-08 \
    --no-wifi \
    --use-existing \
    -y


#####################################################################
# Expose reused SeEDS ndn-tools to Mini-NDN
#####################################################################

if ((REUSE_NDN_TOOLS)); then

    echo
    echo "==> Exposing reused SeEDS ndn-tools in /usr/local/bin"

    for tool in "$SEEDS_NDN_TOOLS_DIR"/bin/*; do
        [[ -x "$tool" ]] || continue

        tool_name="$(basename "$tool")"
        target="/usr/local/bin/$tool_name"

        if [[ -e "$target" && ! -L "$target" ]]; then
            echo "WARNING: Existing file prevents ndn-tools symlink:"
            echo "         $target"
            continue
        fi

        sudo ln -sfn "$tool" "$target"

        echo "==> $target"
        echo "    -> $tool"
    done
fi


#####################################################################
# Expose reused SeEDS NFD to Mini-NDN
#
# Mini-NDN uses helpers such as nfd-start and nfd-stop as well as
# nfd/nfdc. Expose every executable from the reused SeEDS NFD bin/
# directory rather than only the two main binaries.
#####################################################################

if ((REUSE_NFD)); then

    echo
    echo "==> Exposing reused SeEDS NFD in /usr/local/bin"

    for tool in "$SEEDS_NFD_DIR"/bin/*; do
        [[ -x "$tool" ]] || continue

        tool_name="$(basename "$tool")"
        target="/usr/local/bin/$tool_name"

        if [[ -e "$target" && ! -L "$target" ]]; then
            echo "WARNING: Existing file prevents NFD symlink:"
            echo "         $target"
            continue
        fi

        sudo ln -sfn "$tool" "$target"

        echo "==> $target"
        echo "    -> $tool"
    done

    SEEDS_NFD_CONF="$SEEDS_NFD_DIR/etc/ndn/nfd.conf.sample"
    SYSTEM_NFD_CONF="/usr/local/etc/ndn/nfd.conf.sample"

    echo
    echo "==> Exposing reused SeEDS NFD configuration"

    sudo mkdir -p /usr/local/etc/ndn

    if [[ -e "$SYSTEM_NFD_CONF" && ! -L "$SYSTEM_NFD_CONF" ]]; then
        echo "WARNING: Existing file prevents NFD configuration symlink:"
        echo "         $SYSTEM_NFD_CONF"
    else
        sudo ln -sfn "$SEEDS_NFD_CONF" "$SYSTEM_NFD_CONF"
        echo "==> $SYSTEM_NFD_CONF"
        echo "    -> $SEEDS_NFD_CONF"
    fi
fi


#####################################################################
# Enable/start Open vSwitch when available
#####################################################################

echo
echo "==> Checking Open vSwitch"

if command -v ovs-vsctl >/dev/null 2>&1; then

    if systemctl list-unit-files 2>/dev/null \
        | grep -q '^openvswitch\.service'; then

        sudo systemctl enable --now openvswitch
    fi
fi


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

if command -v nlsr >/dev/null 2>&1; then
    echo "==> NLSR installed: $(command -v nlsr)"
else
    echo "WARNING: 'nlsr' was not found in PATH after installation."
fi


#####################################################################
# Show what Mini-NDN actually reused
#####################################################################

echo
echo "==> Dependency reuse"

if ((REUSE_BOOST)); then
    echo "    Boost     : $SEEDS_BOOST_DIR"
else
    echo "    Boost     : Mini-NDN/system"
fi

if ((REUSE_SQLITE)); then
    echo "    SQLite    : $SEEDS_SQLITE_DIR"
else
    echo "    SQLite    : Mini-NDN/system"
fi

if ((REUSE_NDN_CXX)); then
    echo "    ndn-cxx   : $SEEDS_NDN_CXX_DIR"
else
    echo "    ndn-cxx   : Mini-NDN installation"
fi

if ((REUSE_NFD)); then
    echo "    NFD       : $SEEDS_NFD_DIR"
else
    echo "    NFD       : Mini-NDN installation"
fi

if ((REUSE_NDN_TOOLS)); then
    echo "    ndn-tools : $SEEDS_NDN_TOOLS_DIR"
else
    echo "    ndn-tools : Mini-NDN installation"
fi


#####################################################################
# Completed
#####################################################################

echo
echo "============================================================"
echo " Mini-NDN installation completed"
echo "============================================================"
echo

