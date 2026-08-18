#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-${repo_dir}/build/linux-release}"
package_dir="${PACKAGE_DIR:-${repo_dir}}"

cmake -S "${repo_dir}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

cpack --config "${build_dir}/CPackConfig.cmake" -G RPM -B "${package_dir}"

echo "Created Fedora package under ${package_dir}"
