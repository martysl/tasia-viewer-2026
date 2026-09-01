#!/usr/bin/env bash
set -euo pipefail

# Tasia Viewer - Docker Build Helper
# Builds the viewer inside a Docker container with all deps pre-installed.
#
# Usage:
#   ./docker-build.sh              # Build Docker image
#   ./docker-build.sh shell        # Open interactive shell in container
#   ./docker-build.sh build        # Configure + build the viewer
#   ./docker-build.sh configure    # Only configure (autobuild configure)
#
# Prerequisites:
#   - Docker installed
#   - fs-build-variables cloned next to this repo, OR
#     set FS_BUILD_VARIABLES_DIR env var to point to it

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="fs724-fork-builder"
CONTAINER_NAME="fs724-fork-build"

# Default: clone fs-build-variables next to this repo if not specified
: "${FS_BUILD_VARIABLES_DIR:="${SCRIPT_DIR}/../fs-build-variables"}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

build_image() {
    echo -e "${BLUE}Building Docker image '${IMAGE_NAME}'...${NC}"
    docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"
    echo -e "${GREEN}Done!${NC}"
}

run_shell() {
    echo -e "${BLUE}Starting interactive shell...${NC}"
    echo -e "${BLUE}From there, run:${NC}"
    echo -e "  ${GREEN}export AUTOBUILD_VARIABLES_FILE=~/fs-build-variables/variables${NC}"
    echo -e "  ${GREEN}cd ~/tasia-viewer${NC}"
    echo -e "  ${GREEN}autobuild configure -A 64 -c ReleaseFS_open -- --ninja${NC}"
    echo -e "  ${GREEN}autobuild build -A 64 -c ReleaseFS_open ${NC}"
    echo ""

    docker run --rm -it \
        --name "${CONTAINER_NAME}" \
        --hostname tasia-builder \
        -v "${SCRIPT_DIR}:/home/builder/tasia-viewer" \
        -v "${FS_BUILD_VARIABLES_DIR}:/home/builder/fs-build-variables" \
        -e AUTOBUILD_VARIABLES_FILE=/home/builder/fs-build-variables/variables \
        -e XZ_DEFAULTS="-T0" \
        "${IMAGE_NAME}"
}

do_configure() {
    echo -e "${BLUE}Configuring Tasia Viewer build...${NC}"
    docker run --rm -it \
        --name "${CONTAINER_NAME}" \
        -v "${SCRIPT_DIR}:/home/builder/tasia-viewer" \
        -v "${FS_BUILD_VARIABLES_DIR}:/home/builder/fs-build-variables" \
        -e AUTOBUILD_VARIABLES_FILE=/home/builder/fs-build-variables/variables \
        -e XZ_DEFAULTS="-T0" \
        "${IMAGE_NAME}" \
        bash -c "cd ~/tasia-viewer && autobuild configure -A 64 -c ReleaseFS_open -- --ninja"
    echo -e "${GREEN}Configure done! Run './docker-build.sh build' to compile.${NC}"
}

do_build() {
    echo -e "${BLUE}Building Tasia Viewer...${NC}"
    echo -e "${BLUE}(this will take a long time - 1-4 hours depending on hardware)${NC}"

    # Use a named container so we can re-attach
    docker run --rm -it \
        --name "${CONTAINER_NAME}" \
        --hostname tasia-builder \
        -v "${SCRIPT_DIR}:/home/builder/tasia-viewer" \
        -v "${FS_BUILD_VARIABLES_DIR}:/home/builder/fs-build-variables" \
        -e AUTOBUILD_VARIABLES_FILE=/home/builder/fs-build-variables/variables \
        -e XZ_DEFAULTS="-T0" \
        "${IMAGE_NAME}" \
        bash -c "cd ~/tasia-viewer && autobuild build -A 64 -c ReleaseFS_open"

    echo -e "${GREEN}Build complete! Check ${SCRIPT_DIR}/build-linux-x86_64/newview/ for output.${NC}"
}

# --- Main ---

case "${1:-build-image}" in
    build-image|image)
        build_image
        ;;
    shell)
        # Ensure image exists
        docker image inspect "${IMAGE_NAME}" &>/dev/null || build_image
        run_shell
        ;;
    configure)
        docker image inspect "${IMAGE_NAME}" &>/dev/null || build_image
        do_configure
        ;;
    build)
        docker image inspect "${IMAGE_NAME}" &>/dev/null || build_image
        do_build
        ;;
    *)
        echo "Usage: $0 {image|shell|configure|build}"
        echo ""
        echo "  image     - Build Docker image (default)"
        echo "  shell     - Open interactive shell in container"
        echo "  configure - Run autobuild configure step"
        echo "  build     - Configure + build the viewer"
        exit 1
        ;;
esac
