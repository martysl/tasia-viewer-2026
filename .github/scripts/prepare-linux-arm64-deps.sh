#!/usr/bin/env bash
set -euo pipefail

workspace="${GITHUB_WORKSPACE:-$(pwd)}"
build_dir="${workspace}/build-linux-aarch64"
packages_dir="${build_dir}/packages"
work_dir="${RUNNER_TEMP:-/tmp}/tasia-linux-arm64-deps"

if [[ "$(uname -m)" != "aarch64" && "$(uname -m)" != "arm64" ]]; then
    echo "This dependency preparation must run natively on ARM64." >&2
    exit 1
fi

rm -rf "${work_dir}"
mkdir -p "${work_dir}" "${packages_dir}/lib/release" \
    "${packages_dir}/include" "${packages_dir}/ffmpeg"

CEF_URL="https://cef-builds.spotifycdn.com/cef_binary_139.0.40%2Bg465474a%2Bchromium-139.0.7258.139_linuxarm64_minimal.tar.bz2"
CEF_SHA256="da99ba0dd4e4a72545e7ff859e029b2c09e1b009339f6248b8b0995241dbc8ee"
CEF_ARCHIVE="${work_dir}/cef-linuxarm64.tar.bz2"

curl --fail --location --retry 3 --output "${CEF_ARCHIVE}" "${CEF_URL}"
printf '%s  %s\n' "${CEF_SHA256}" "${CEF_ARCHIVE}" | sha256sum --check --strict

git clone --depth 1 --branch v1.26.0-CEF_139.0.40 \
    https://github.com/secondlife/dullahan.git "${work_dir}/dullahan"

# Dullahan's CMake reads the autobuild 64-bit flags from the environment and
# injects x86-only flags (-m64/-march=x86-64) which are invalid on aarch64.
# Build Dullahan with architecture-neutral flags.
unset LL_BUILD LL_BUILD_RELEASE AUTOBUILD_ADDRSIZE 2>/dev/null || true
# CEF's cef_variables.cmake detects "arm64" only (macOS naming) and falls back
# to x86_64 on Linux aarch64, injecting -m64/-march=x86-64. Force PROJECT_ARCH.
cmake -S "${work_dir}/dullahan" -B "${work_dir}/dullahan-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${work_dir}/dullahan-stage" \
    -DCMAKE_C_FLAGS="" \
    -DCMAKE_CXX_FLAGS="" \
    -DPROJECT_ARCH=arm64 \
    -DUSE_SPOTIFY_CEF=TRUE \
    -DSPOTIFY_CEF_URL="file://${CEF_ARCHIVE}"
cmake --build "${work_dir}/dullahan-build" --parallel "$(nproc)"
cmake --install "${work_dir}/dullahan-build"

mkdir -p "${work_dir}/dullahan-stage/include/cef" \
    "${work_dir}/dullahan-stage/LICENSES"
cp "${work_dir}/dullahan/src/dullahan.h" \
   "${work_dir}/dullahan/src/dullahan_version.h" \
   "${work_dir}/dullahan-stage/include/cef/"
cp "${work_dir}/dullahan/CEF_LICENSE.txt" \
   "${work_dir}/dullahan/LICENSE.txt" \
   "${work_dir}/dullahan-stage/LICENSES/"
rm -f "${work_dir}/dullahan-stage/bin/release/"*.bin \
      "${work_dir}/dullahan-stage/bin/release/"*.so* \
      "${work_dir}/dullahan-stage/bin/release/"*.json \
      "${work_dir}/dullahan-stage/lib/release/chrome-sandbox"
cp -a "${work_dir}/dullahan-stage/." "${packages_dir}/"

WEBRTC_URL="https://github.com/crow-misia/libwebrtc-bin/releases/download/139.7258.3.1/libwebrtc-linux-arm64.tar.xz"
WEBRTC_SHA256="45e20a15b179d1cd6fae5dcbb967a0389bb5abdbac03248e7efe48b7f065ae1c"
WEBRTC_ARCHIVE="${work_dir}/libwebrtc-linux-arm64.tar.xz"
WEBRTC_EXTRACT="${work_dir}/webrtc"

curl --fail --location --retry 3 --output "${WEBRTC_ARCHIVE}" "${WEBRTC_URL}"
printf '%s  %s\n' "${WEBRTC_SHA256}" "${WEBRTC_ARCHIVE}" | sha256sum --check --strict
mkdir -p "${WEBRTC_EXTRACT}" "${packages_dir}/include/webrtc"
tar -xJf "${WEBRTC_ARCHIVE}" -C "${WEBRTC_EXTRACT}"
cp -a "${WEBRTC_EXTRACT}/include/." "${packages_dir}/include/webrtc/"
cp "${WEBRTC_EXTRACT}/lib/libwebrtc.a" "${packages_dir}/lib/release/"

FFMPEG_URL="https://johnvansickle.com/ffmpeg/releases/ffmpeg-7.0.2-arm64-static.tar.xz"
FFMPEG_SHA256="f4149bb2b0784e30e99bdda85471c9b5930d3402014e934a5098b41d0f7201b1"
FFMPEG_ARCHIVE="${work_dir}/ffmpeg-arm64.tar.xz"
FFMPEG_EXTRACT="${work_dir}/ffmpeg"

curl --fail --location --retry 3 --output "${FFMPEG_ARCHIVE}" "${FFMPEG_URL}"
printf '%s  %s\n' "${FFMPEG_SHA256}" "${FFMPEG_ARCHIVE}" | sha256sum --check --strict
mkdir -p "${FFMPEG_EXTRACT}"
tar -xJf "${FFMPEG_ARCHIVE}" -C "${FFMPEG_EXTRACT}"
ffmpeg_bin="$(find "${FFMPEG_EXTRACT}" -maxdepth 2 -type f -name ffmpeg -print -quit)"
test -n "${ffmpeg_bin}"
install -m 0755 "${ffmpeg_bin}" "${packages_dir}/ffmpeg/tasia-ffmpeg"

# Stage the non-core distribution libraries linked by the ARM build. Keeping
# these in packages/lib/release lets the existing manifest produce a portable
# tree without changing the x86_64 dependency policy.
system_lib_dir="/usr/lib/aarch64-linux-gnu"
if [[ ! -d "${system_lib_dir}" ]]; then
    echo "Missing ARM64 system library directory: ${system_lib_dir}" >&2
    exit 1
fi

runtime_patterns=(
    libapr-1.so* libaprutil-1.so* libboost_atomic.so* libboost_chrono.so*
    libboost_context.so* libboost_fiber.so* libboost_filesystem.so*
    libboost_program_options.so* libboost_regex.so* libboost_thread.so*
    libboost_url.so* libboost_wave.so* libcurl.so* libssl.so* libcrypto.so*
    libnghttp2.so* libz.so* libexpat.so* libfreetype.so* libpng16.so*
    libjpeg.so* libhunspell-1.7.so* libuuid.so* libSDL2.so* libalut.so*
    libopenal.so* libopenjp2.so* libvorbis.so* libvorbisenc.so* libogg.so*
    libxml2.so* libminizip.so* libxxhash.so*
)
shopt -s nullglob
for pattern in "${runtime_patterns[@]}"; do
    matches=("${system_lib_dir}"/${pattern})
    if (( ${#matches[@]} )); then
        cp -a "${matches[@]}" "${packages_dir}/lib/release/"
    fi
done
shopt -u nullglob
install -m 0644 /etc/ssl/certs/ca-certificates.crt "${packages_dir}/ca-bundle.crt"

for required in \
    "${packages_dir}/lib/release/libcef.so" \
    "${packages_dir}/lib/release/libdullahan.a" \
    "${packages_dir}/lib/release/libcef_dll_wrapper.a" \
    "${packages_dir}/lib/release/libwebrtc.a" \
    "${packages_dir}/bin/release/dullahan_host" \
    "${packages_dir}/ffmpeg/tasia-ffmpeg"; do
    test -e "${required}" || { echo "Missing staged ARM dependency: ${required}" >&2; exit 1; }
done

file "${packages_dir}/lib/release/libcef.so" \
     "${packages_dir}/bin/release/dullahan_host" \
     "${packages_dir}/ffmpeg/tasia-ffmpeg"

if file "${packages_dir}/lib/release/libcef.so" \
        "${packages_dir}/bin/release/dullahan_host" \
        "${packages_dir}/ffmpeg/tasia-ffmpeg" | grep -Eq 'x86-64|Intel 80386'; then
    echo "An x86 binary was staged in the ARM dependency set." >&2
    exit 1
fi

echo "ARM64 CEF/Dullahan, WebRTC and FFmpeg dependencies are staged in ${packages_dir}."
