#!/bin/zsh
# Собирает «Nextcloud Fork.app»: клиент Nextcloud 35, в котором диск в Finder («Места», File Provider)
# работает вместе с обычной синхронизацией выбранных папок.
# NO_INSTALL=1 ./build.sh — только собрать (в $BUILD_DIR/package).
# SKIP_EXTENSIONS=1 — без расширений Finder и File Provider (быстрее, для проверки кода клиента).
#
# Зависимости — из Homebrew (список в BREW_DEPS), KDSingleApplication в Homebrew нет — собирается здесь же.
# Сборка весит несколько гигабайт, поэтому лежит в ~/Library/Caches, а не рядом с исходниками.
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-$HOME/Library/Caches/RomanenkoSources/nextcloud-fork}"
BREW_DEPS=(cmake ninja pkgconf qtbase qtdeclarative qtsvg qt5compat qtwebsockets qttools qtshadertools qtimageformats
  qtkeychain karchive extra-cmake-modules libp11 librsvg openssl@3)
KDSA_TAG=v1.2.1
KDSA_COMMIT=b63420aebb78df20e37310bb090f075990a5a4f6

BREW="$(command -v brew || echo /opt/homebrew/bin/brew)"
[[ -x "$BREW" ]] || { echo "Нужен Homebrew"; exit 1; }
missing=()
for dep in "${BREW_DEPS[@]}"; do "$BREW" list --versions "$dep" >/dev/null 2>&1 || missing+=("$dep"); done
if (( ${#missing} )); then
  echo "Не хватает пакетов Homebrew. Поставьте: brew install ${missing[*]}"
  exit 1
fi
HB="$("$BREW" --prefix)"
export PATH="$HB/bin:$PATH"
export PKG_CONFIG_PATH="$HB/opt/openssl@3/lib/pkgconfig:$HB/lib/pkgconfig"
export QT_QPA_PLATFORM=offscreen
LOG="$BUILD_DIR/logs"
mkdir -p "$LOG"

KDSA_PREFIX="$BUILD_DIR/deps/kdsingleapplication-$KDSA_TAG"
if [[ ! -f "$KDSA_PREFIX/lib/cmake/KDSingleApplication-qt6/KDSingleApplication-qt6Config.cmake" ]]; then
  echo "• KDSingleApplication $KDSA_TAG"
  src="$BUILD_DIR/deps/src/kdsingleapplication"
  rm -rf "$src" && git clone -q --depth 1 --branch "$KDSA_TAG" https://github.com/KDAB/KDSingleApplication.git "$src"
  [[ "$(git -C "$src" rev-parse HEAD)" == "$KDSA_COMMIT" ]] || { echo "KDSingleApplication $KDSA_TAG: не тот коммит"; exit 1; }
  cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DKDSingleApplication_QT6=ON \
    -DKDSingleApplication_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX="$KDSA_PREFIX" -DCMAKE_PREFIX_PATH="$HB" > "$LOG/kdsa.log" 2>&1
  cmake --build "$src/build" --target install >> "$LOG/kdsa.log" 2>&1 || { tail -n 30 "$LOG/kdsa.log"; exit 1; }
fi

echo "• Тесты режимов синхронизации"
cmake -S test/hybrid -B "$BUILD_DIR/policy" -G Ninja -DCMAKE_PREFIX_PATH="$HB" > "$LOG/policy.log" 2>&1 \
  || { tail -n 30 "$LOG/policy.log"; exit 1; }
cmake --build "$BUILD_DIR/policy" >> "$LOG/policy.log" 2>&1 || { tail -n 30 "$LOG/policy.log"; exit 1; }
ctest --test-dir "$BUILD_DIR/policy" --output-on-failure

echo "• Настройка сборки"
if ! cmake -S . -B "$BUILD_DIR/client" -G Ninja \
    -DNEXTCLOUD_FORK=ON -DNEXTCLOUD_EXPERIMENTAL_HYBRID_SYNC=ON \
    -DNEXTCLOUD_FORK_UNSIGNED=ON -DBUILD_FILE_PROVIDER_MODULE=ON -DBUILD_UPDATER=OFF -DBUILD_TESTING=OFF \
    -DBUILD_OWNCLOUD_OSX_BUNDLE=ON -DCMAKE_OSX_ARCHITECTURES=arm64 -DMACOS_USE_ICON_COMPOSER=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$KDSA_PREFIX;$HB;$HB/opt/openssl@3" \
    -DSVG_CONVERTER="$HB/bin/rsvg-convert" \
    -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/package" > "$LOG/configure.log" 2>&1; then
  tail -n 40 "$LOG/configure.log"
  exit 1
fi

# Полная сборка — цель all: расширения и плагины VFS, которые нужны cmake --install.
TARGETS=(all)
[[ "${SKIP_EXTENSIONS:-}" == 1 ]] && TARGETS=(nextcloud)
echo "• Сборка: ${TARGETS[*]} (журнал: $LOG/build.log)"
if ! cmake --build "$BUILD_DIR/client" --target "${TARGETS[@]}" -j "${JOBS:-6}" > "$LOG/build.log" 2>&1; then
  grep -E "error:|FAILED:" "$LOG/build.log" | head -n 30
  tail -n 20 "$LOG/build.log"
  exit 1
fi

if [[ "${SKIP_EXTENSIONS:-}" == 1 ]]; then
  echo "Собрано без расширений: $BUILD_DIR/client/bin"
  exit 0
fi

echo "• Сборка пакета"
cmake --install "$BUILD_DIR/client" > "$LOG/package.log" 2>&1 || { tail -n 30 "$LOG/package.log"; exit 1; }
APP="$BUILD_DIR/package/Nextcloud Fork.app"
[[ -d "$APP" ]] || { echo "Нет пакета: $APP"; exit 1; }

if [[ "${NO_INSTALL:-}" == 1 ]]; then
  echo "Собрано: $APP"
  exit 0
fi

# Без подписи Apple Development расширение File Provider не загрузится: ставить пока рано.
echo "Установка не делается: нужна подпись Apple Development (см. FORK.md). Собрано: $APP"
exit 1
