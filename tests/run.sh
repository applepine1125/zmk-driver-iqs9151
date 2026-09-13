#!/usr/bin/env bash
# ZMK の CI コンテナで ztest を実行する。
# 使い方: tests/run.sh [テストディレクトリ名...]   (省略時は tests/ 配下すべて)
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="docker.io/zmkfirmware/zmk-build-arm:3.5"
VOLUME="zmk-driver-iqs9151-west"
BOARD="native_sim_64"

if [ "$#" -eq 0 ]; then
  set -- $(cd "$REPO_DIR/tests" && ls -d */ | tr -d /)
fi

docker volume create "$VOLUME" >/dev/null
docker run --rm \
  -v "$VOLUME:/ws" \
  -v "$REPO_DIR:/ws/modules/zmk-driver-iqs9151" \
  -w /ws \
  "$IMAGE" bash -ec '
    # `west init -l modules/zmk-driver-iqs9151` は topdir をリポジトリ直上の
    # 親ディレクトリ(modules/)にしてしまい、CMakeLists.txt が前提とする
    # 「topdir 直下に zmk/ を置く」レイアウトと合わなくなるため、
    # .west/config を直接作成して topdir をワークスペースルートに固定する。
    if [ ! -d .west ]; then
      mkdir -p .west
      cat > .west/config << "EOC"
[manifest]
path = modules/zmk-driver-iqs9151
file = west.yml
EOC
    fi
    west update --fetch-opt=--filter=tree:0
    west zephyr-export
    status=0
    for t in "$@"; do
      echo "=== $t ==="
      west build -p always -b '"$BOARD"' -d "build/$t" "modules/zmk-driver-iqs9151/tests/$t" -t run || status=1
    done
    exit $status
  ' _ "$@"
