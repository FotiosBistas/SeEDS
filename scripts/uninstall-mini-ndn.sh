#!/usr/bin/env bash

set -uo pipefail

#####################################################################
# Paths
#####################################################################

SEEDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MININDN_DIR="$SEEDS_ROOT/external/mini-ndn"
DL_DIR="$MININDN_DIR/dl"

FAILURES=0


#####################################################################
# Helpers
#####################################################################

uninstall_waf()
{
    local name="$1"
    local dir="$2"

    echo
    echo "==> Uninstalling $name"

    if [[ ! -d "$dir" ]]; then
        echo "WARNING: Source directory does not exist:"
        echo "         $dir"
        FAILURES=$((FAILURES + 1))
        return
    fi

    if [[ ! -f "$dir/waf" ]]; then
        echo "WARNING: waf not found:"
        echo "         $dir/waf"
        FAILURES=$((FAILURES + 1))
        return
    fi

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
echo "==> Uninstalling infoedit"

if [[ -f "$DL_DIR/infoedit/Makefile" ]]; then
    (
        cd "$DL_DIR/infoedit"
        sudo make uninstall
    ) || {
        echo "WARNING: Failed to uninstall infoedit"
        FAILURES=$((FAILURES + 1))
    }
else
    echo "WARNING: infoedit Makefile not found"
    FAILURES=$((FAILURES + 1))
fi


#####################################################################
# Uninstall old OpenFlow reference implementation
#
# This removes binaries such as:
#   controller
#   ofdatapath
#   ofprotocol
#####################################################################

echo
echo "==> Uninstalling OpenFlow reference implementation"

if [[ -f "$DL_DIR/openflow/Makefile" ]]; then
    (
        cd "$DL_DIR/openflow"
        sudo make uninstall
    ) || {
        echo "WARNING: Failed to uninstall OpenFlow"
        FAILURES=$((FAILURES + 1))
    }
else
    echo "WARNING: OpenFlow Makefile not found"
    FAILURES=$((FAILURES + 1))
fi


#####################################################################
# Uninstall Mini-NDN and Mininet Python packages
#####################################################################

echo
echo "==> Uninstalling Mini-NDN Python package"

sudo python3 -m pip uninstall -y Mini-NDN 2>/dev/null || true

echo
echo "==> Uninstalling Mininet Python package"

sudo python3 -m pip uninstall -y mininet 2>/dev/null || true


#####################################################################
# Remove Mini-NDN's Mininet helper copies
#
# Mini-NDN's installer explicitly creates /usr/local/bin/mn.mininet.
#####################################################################

echo
echo "==> Removing Mini-NDN Mininet helper binaries"

sudo rm -f \
    /usr/local/bin/mn.mininet \
    /usr/local/bin/mn.wifi


#####################################################################
# Remove Mini-NDN configuration
#
# Mini-NDN itself installs topology configuration files here.
#####################################################################

echo
echo "==> Removing Mini-NDN configuration"

sudo rm -rf /usr/local/etc/mini-ndn


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


echo
echo "============================================================"
echo " Remaining NDN libraries"
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

#####################################################################
# Remove downloaded Mini-NDN dependency sources
#
# Some build/install steps run as root and can leave root-owned
# __pycache__, build/, egg-info, and other generated files inside dl/.
# The whole dl/ directory is disposable, so remove it with sudo.
#####################################################################

echo
echo "==> Removing downloaded Mini-NDN dependencies"

if [[ -d "$DL_DIR" ]]; then

    # Safety check before using sudo rm -rf.
    if [[ "$DL_DIR" != "$MININDN_DIR/dl" || -z "$DL_DIR" ]]; then
        echo "ERROR: Refusing to remove unexpected directory:"
        echo "       $DL_DIR"
        exit 1
    fi

    if sudo rm -rf -- "$DL_DIR"; then
        echo "==> Removed:"
        echo "    $DL_DIR"
    else
        echo "ERROR: Failed to remove:"
        echo "       $DL_DIR"
        exit 1
    fi

else
    echo "==> Dependency download directory already absent"
fi
