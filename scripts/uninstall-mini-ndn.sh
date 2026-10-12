#!/usr/bin/env bash

set -uo pipefail

trap '
    echo
    echo "ERROR: Mini-NDN uninstall interrupted."
    exit 130
' INT TERM

#####################################################################
# Detect Linux distribution
#####################################################################

if [[ -r /etc/os-release ]]; then
    . /etc/os-release
else
    ID=""
    VERSION_ID=""
fi

#####################################################################
# Paths
#####################################################################

SEEDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MININDN_DIR="$SEEDS_ROOT/external/mini-ndn"
DL_DIR="$MININDN_DIR/dl"

#
# SeEDS/build may contain dependencies reused by Mini-NDN through
# --use-existing.
#
# This uninstall script must NEVER modify anything in SeEDS/build.
#
SEEDS_BUILD_DIR="$SEEDS_ROOT/build"

SEEDS_NDN_TOOLS_DIR="$SEEDS_BUILD_DIR/ndn-tools/install"
SEEDS_NFD_DIR="$SEEDS_BUILD_DIR/NFD/install"

FAILURES=0


#####################################################################
# Safety checks
#####################################################################

#
# The Mini-NDN download directory must remain completely separate
# from SeEDS/build because SeEDS dependencies may have been reused
# by Mini-NDN.
#
if [[ "$DL_DIR" == "$SEEDS_BUILD_DIR"* ]]; then
    echo "ERROR: Mini-NDN download directory unexpectedly points"
    echo "       inside the SeEDS build directory:"
    echo
    echo "       $DL_DIR"
    echo
    exit 1
fi

#
# Additional protection before any later sudo rm -rf.
#
EXPECTED_DL_DIR="$MININDN_DIR/dl"

if [[ "$DL_DIR" != "$EXPECTED_DL_DIR" || -z "$DL_DIR" ]]; then
    echo "ERROR: Unexpected Mini-NDN download directory:"
    echo
    echo "       $DL_DIR"
    echo
    exit 1
fi


#####################################################################
# Helpers
#####################################################################

uninstall_waf()
{
    local name="$1"
    local dir="$2"

    echo
    echo "==> Checking $name"

    #
    # A missing dependency source tree is now valid.
    #
    # Mini-NDN may have reused a compatible dependency from
    # SeEDS/build through --use-existing, in which case Mini-NDN
    # never downloaded or installed its own copy.
    #
    if [[ ! -d "$dir" ]]; then
        echo "==> $name source tree not present"
        echo "    Reused externally, already removed, or never installed"
        return
    fi

    #
    # Likewise, if this directory does not contain waf, there is no
    # Waf installation manifest here that we can uninstall.
    #
    if [[ ! -f "$dir/waf" ]]; then
        echo "==> $name does not contain waf"
        echo "    Skipping source uninstall"
        return
    fi

    echo "==> Uninstalling Mini-NDN-owned $name"

    (
        cd "$dir"
        sudo python3 ./waf uninstall
    ) || {
        echo "WARNING: Failed to uninstall $name"
        FAILURES=$((FAILURES + 1))
    }
}


#####################################################################
# Banner
#####################################################################

echo
echo "============================================================"
echo " Mini-NDN uninstall for SeEDS"
echo "============================================================"
echo

echo "Mini-NDN directory:"
echo "  $MININDN_DIR"

echo
echo "Dependency source directory:"
echo "  $DL_DIR"

echo
echo "Protected SeEDS build directory:"
echo "  $SEEDS_BUILD_DIR"


echo
echo "==> Acquiring sudo credentials"

if ! sudo -v; then
    echo "ERROR: sudo authentication failed"
    exit 1
fi


#####################################################################
# Clean running Mininet / Mini-NDN state
#####################################################################

echo
echo "==> Cleaning Mininet runtime state"

if command -v mn >/dev/null 2>&1; then
    sudo mn -c || true
fi

sudo rm -rf /tmp/minindn


#####################################################################
# Uninstall source-built NDN applications
#
# Remove dependants before their libraries.
#
# Missing source trees are NOT errors because Mini-NDN may have
# reused those components from SeEDS/build.
#####################################################################

uninstall_waf \
    "NDN Traffic Generator" \
    "$DL_DIR/ndn-traffic-generator"

uninstall_waf \
    "NDN Essential Tools" \
    "$DL_DIR/ndn-tools"

uninstall_waf \
    "NLSR" \
    "$DL_DIR/NLSR"

uninstall_waf \
    "NFD" \
    "$DL_DIR/NFD"

uninstall_waf \
    "PSync" \
    "$DL_DIR/PSync"

uninstall_waf \
    "ndn-cxx" \
    "$DL_DIR/ndn-cxx"


#####################################################################
# Uninstall infoedit
#####################################################################

echo
echo "==> Checking infoedit"

if [[ -f "$DL_DIR/infoedit/Makefile" ]]; then

    echo "==> Uninstalling infoedit"

    (
        cd "$DL_DIR/infoedit"
        sudo make uninstall
    ) || {
        echo "WARNING: Failed to uninstall infoedit"
        FAILURES=$((FAILURES + 1))
    }

else
    echo "==> infoedit source tree not present"
    echo "    Already removed or never installed"
fi


#####################################################################
# Uninstall old OpenFlow reference implementation
#
# Mininet may build the old OpenFlow reference implementation.
#
# This installs binaries including:
#
#   controller
#   ofdatapath
#   ofprotocol
#
#####################################################################

echo
echo "==> Checking OpenFlow reference implementation"

if [[ -f "$DL_DIR/openflow/Makefile" ]]; then

    echo "==> Uninstalling OpenFlow reference implementation"

    (
        cd "$DL_DIR/openflow"
        sudo make uninstall
    ) || {
        echo "WARNING: OpenFlow make uninstall failed"
        FAILURES=$((FAILURES + 1))
    }

else
    echo "==> OpenFlow source tree not present"
    echo "    Already removed or never installed"
fi

#####################################################################
# Remove residual Mini-NDN-owned NDN applications
#
# --use-existing can cause Mini-NDN to skip downloading a source tree
# when an earlier /usr/local installation already exists. Therefore
# source-directory absence cannot be used as proof that the installed
# component belongs to somebody else.
#####################################################################

echo
echo "==> Removing residual Mini-NDN-owned NDN applications"

sudo rm -f \
    /usr/local/bin/nlsr \
    /usr/local/bin/nlsrc \
    /usr/local/bin/ndn-traffic-client \
    /usr/local/bin/ndn-traffic-server \
    /usr/local/bin/infoconv \
    /usr/local/bin/infoedit


#####################################################################
# Remove residual Mini-NDN-owned PSync installation
#####################################################################

echo
echo "==> Removing residual PSync installation"

sudo rm -f \
    /usr/local/lib/libPSync.so* \
    /usr/local/lib64/libPSync.so* \
    /usr/local/lib/pkgconfig/PSync.pc \
    /usr/local/lib64/pkgconfig/PSync.pc

sudo rm -rf \
    /usr/local/include/PSync


#####################################################################
# Remove NLSR configuration installed by Mini-NDN
#####################################################################

echo
echo "==> Removing Mini-NDN NLSR configuration"

sudo rm -f \
    /usr/local/etc/ndn/nlsr.conf.sample


#####################################################################
# Remove SeEDS library exposure used by Mini-NDN
#####################################################################

echo
echo "==> Removing Mini-NDN dynamic linker configuration"

sudo rm -f \
    /etc/ld.so.conf.d/seeds-mini-ndn.conf

#####################################################################
# Remove residual OpenFlow reference binaries
#
# The old OpenFlow build does not reliably remove these with
# make uninstall.
#
# These are installed by Mininet into /usr/local/bin.
#####################################################################

echo
echo "==> Removing residual OpenFlow reference binaries"

sudo rm -f \
    /usr/local/bin/controller \
    /usr/local/bin/ofdatapath \
    /usr/local/bin/ofprotocol




#####################################################################
# Uninstall Mini-NDN and Mininet Python packages
#
# Ubuntu 24.04 implements PEP 668. These packages were installed
# under /usr/local by their source installers, so pip needs the
# explicit override in order to remove them again.
#####################################################################

echo
echo "==> Uninstalling Mini-NDN Python package"

if [[ "$ID" == "ubuntu" ]] &&
   dpkg --compare-versions "${VERSION_ID:-0}" ge "24.04"; then

    sudo python3 -m pip uninstall \
        --break-system-packages \
        -y Mini-NDN || true

else

    sudo python3 -m pip uninstall \
        -y Mini-NDN || true

fi


echo
echo "==> Uninstalling Mininet Python package"

if [[ "$ID" == "ubuntu" ]] &&
   dpkg --compare-versions "${VERSION_ID:-0}" ge "24.04"; then

    sudo python3 -m pip uninstall \
        --break-system-packages \
        -y mininet || true

else

    sudo python3 -m pip uninstall \
        -y mininet || true

fi


#####################################################################
# Remove Mini-NDN's Mininet helper copies
#
# Mini-NDN's installer may create helper copies such as:
#
#   /usr/local/bin/mn.mininet
#   /usr/local/bin/mn.wifi
#####################################################################

echo
echo "==> Removing Mini-NDN Mininet helper binaries"

sudo rm -f \
    /usr/local/bin/mn.mininet \
    /usr/local/bin/mn.wifi


#####################################################################
# Remove symlinks to reused SeEDS ndn-tools
#
# When Mini-NDN reuses ndn-tools from SeEDS/build, the installer
# exposes those binaries under /usr/local/bin using symbolic links.
#
# Remove only links that resolve into the SeEDS ndn-tools
# installation. Never remove the actual SeEDS binaries.
#####################################################################

echo
echo "==> Removing reused SeEDS ndn-tools symlinks"

for target in /usr/local/bin/*; do

    [[ -L "$target" ]] || continue

    link_target="$(readlink -f "$target" 2>/dev/null || true)"

    if [[ "$link_target" == "$SEEDS_NDN_TOOLS_DIR/bin/"* ]]; then

        echo "==> Removing:"
        echo "    $target"
        echo "    -> $link_target"

        sudo rm -f "$target"
    fi

done


#####################################################################
# Remove symlinks to reused SeEDS NFD
#
# When Mini-NDN reuses the NFD already built by SeEDS, the installer
# exposes:
#
#   /usr/local/bin/nfd
#   /usr/local/bin/nfdc
#   /usr/local/etc/ndn/nfd.conf.sample
#
# as symbolic links into SeEDS/build/NFD/install.
#
# Remove only those symbolic links. Never remove the actual SeEDS
# NFD installation.
#####################################################################

echo
echo "==> Removing reused SeEDS NFD symlinks"

for target in \
    /usr/local/bin/nfd \
    /usr/local/bin/nfdc \
    /usr/local/etc/ndn/nfd.conf.sample
do

    [[ -L "$target" ]] || continue

    link_target="$(readlink -f "$target" 2>/dev/null || true)"

    if [[ "$link_target" == "$SEEDS_NFD_DIR/"* ]]; then

        echo "==> Removing:"
        echo "    $target"
        echo "    -> $link_target"

        sudo rm -f "$target"
    fi

done


#####################################################################
# Remove Mini-NDN configuration
#
# Only Mini-NDN's own configuration directory is removed.
#
# This does NOT touch:
#
#   SeEDS/build/NFD/...
#   SeEDS/build/ndn-cxx/...
#   SeEDS/build/ndn-tools/...
#
#####################################################################

echo
echo "==> Removing Mini-NDN configuration"

sudo rm -rf /usr/local/etc/mini-ndn

#####################################################################
# Remove downloaded Mini-NDN dependency sources
#
# This MUST happen after all waf/make uninstall operations because
# those operations need their original source/build trees.
#
# Some Mini-NDN installation steps execute under sudo and can leave
# root-owned files inside dl/, such as:
#
#   __pycache__/
#   build/
#   *.egg-info/
#
# Therefore sudo is required to reliably remove the complete dl/
# directory.
#
#####################################################################

echo
echo "==> Removing downloaded Mini-NDN dependencies"

if [[ -d "$DL_DIR" ]]; then

    #
    # Repeat the safety check immediately before sudo rm -rf.
    #
    if [[ "$DL_DIR" != "$MININDN_DIR/dl" || -z "$DL_DIR" ]]; then
        echo "ERROR: Refusing to remove unexpected directory:"
        echo "       $DL_DIR"
        exit 1
    fi

    if [[ "$DL_DIR" == "$SEEDS_BUILD_DIR"* ]]; then
        echo "ERROR: Refusing to remove anything inside SeEDS/build:"
        echo "       $DL_DIR"
        exit 1
    fi

    if sudo rm -rf -- "$DL_DIR"; then
        echo "==> Removed:"
        echo "    $DL_DIR"
    else
        echo "ERROR: Failed to remove:"
        echo "       $DL_DIR"

        FAILURES=$((FAILURES + 1))
    fi

else
    echo "==> Dependency download directory already absent"
fi


#####################################################################
# Refresh dynamic linker cache
#####################################################################

echo
echo "==> Refreshing dynamic linker cache"

sudo ldconfig


#####################################################################
# Refresh shell command cache
#####################################################################

hash -r


#####################################################################
# Audit
#####################################################################

echo
echo "============================================================"
echo " Remaining Mini-NDN / NDN commands"
echo "============================================================"
echo

for x in \
    nfd nfdc \
    nlsr nlsrc \
    ndnsec \
    ndnping ndnpingserver \
    ndnputchunks ndncatchunks \
    ndn-traffic-client \
    infoconv infoedit \
    mn \
    controller ofdatapath ofprotocol
do
    printf "%-22s " "$x"

    if command -v "$x" >/dev/null 2>&1; then
        command -v "$x"
    else
        echo "not found"
    fi
done


#####################################################################
# Audit remaining Mini-NDN-installed libraries/headers
#
# We intentionally inspect only /usr/local.
#
# SeEDS/build is NOT inspected here because dependencies in that tree
# belong to SeEDS and must remain installed.
#####################################################################

echo
echo "============================================================"
echo " Remaining NDN libraries / headers under /usr/local"
echo "============================================================"
echo

find \
    /usr/local/lib \
    /usr/local/lib64 \
    /usr/local/include \
    -maxdepth 2 \
    \( \
        -iname '*ndn*' \
        -o -iname '*psync*' \
        -o -iname '*openflow*' \
    \) \
    -print 2>/dev/null || true


#####################################################################
# Verify Mini-NDN download directory removal
#####################################################################

echo
echo "============================================================"
echo " Mini-NDN download directory"
echo "============================================================"
echo

if [[ -e "$DL_DIR" ]]; then
    echo "WARNING: Mini-NDN download directory still exists:"
    echo "         $DL_DIR"

    FAILURES=$((FAILURES + 1))
else
    echo "Removed:"
    echo "  $DL_DIR"
fi


#####################################################################
# Verify SeEDS build was preserved
#####################################################################

echo
echo "============================================================"
echo " SeEDS build"
echo "============================================================"
echo

if [[ -d "$SEEDS_BUILD_DIR" ]]; then
    echo "Preserved:"
    echo "  $SEEDS_BUILD_DIR"
else
    echo "NOTE: SeEDS build directory does not currently exist:"
    echo "      $SEEDS_BUILD_DIR"
fi


#####################################################################
# Completed
#####################################################################

echo
echo "============================================================"

if ((FAILURES == 0)); then

    echo " Mini-NDN uninstall completed"

else

    echo " Uninstall completed with $FAILURES warning(s)"
    echo " Review the audit above for remaining files."

fi

echo "============================================================"
echo
