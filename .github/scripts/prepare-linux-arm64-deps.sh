#!/usr/bin/env bash
# Prepare native ARM64 dependencies for CI build.
# On a native ARM64 runner all system packages are installed via apt-get
# in the "Install build tools and ARM system dependencies" step.
# This script is a placeholder for any ARM64-specific post-install
# tweaks (e.g. patchelf, library symlinks) that may be needed later.
set -euo pipefail
echo "Prepare native ARM64 dependencies: native runner, no extra prep needed."
echo "  arch: $(uname -m)"
echo "  os:   $(lsb_release -d -s 2>/dev/null || cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2 | tr -d '"')"
