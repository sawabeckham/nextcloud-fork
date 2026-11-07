#!/bin/zsh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
# Собирает «Nextcloud Fork.app»: клиент Nextcloud 35, в котором диск в Finder («Места», File Provider)
# работает вместе с обычной синхронизацией выбранных папок.
# ./build.sh — собрать подписанное приложение и PKG; установка запускается вручную.
# NO_INSTALL=1 сохранён для совместимости: сборка никогда не запускает установку автоматически.
# SKIP_EXTENSIONS=1 — без расширений Finder и File Provider (быстрее, для проверки кода клиента).
#
# Зависимости — из Homebrew (список в BREW_DEPS), KDSingleApplication в Homebrew нет — собирается здесь же.
# Сборка весит несколько гигабайт, поэтому лежит в ~/Library/Caches, а не рядом с исходниками.
set -euo pipefail
cd "$(dirname "$0")"

fork_version="$(sed -n 's/^set(NEXTCLOUD_FORK_VERSION "\([0-9.]*\)")/\1/p' FORK_VERSION.cmake)"
BUILD_DIR="${BUILD_DIR:-$HOME/Library/Caches/RomanenkoSources/nextcloud-fork/releases/$fork_version}"
# Подпись: Personal Team Apple ID в Xcode (сертификат «Apple Development»). UNSIGNED=1 — собрать без подписи.
TEAM_ID="${TEAM_ID:-EW76LLFDSH}"
[[ "${UNSIGNED:-}" == 1 ]] && TEAM_ID=""
BREW_DEPS=(cmake ninja pkgconf qtbase qtdeclarative qtsvg qt5compat qtwebsockets qttools qtshadertools qtimageformats
  qtkeychain karchive extra-cmake-modules libp11 librsvg openssl@3)
# Security redirect regression tests require Qt's local HTTP server fixture.
[[ "${CLIENT_TESTING:-OFF}" == ON ]] && BREW_DEPS+=(qthttpserver)
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

IDENTITY=""
if [[ -n "$TEAM_ID" ]]; then
  # Сертификат выбирается по Team ID (поле OU), подпись — по SHA-1: имя «Apple Development: …» неоднозначно.
  valid="$(security find-identity -v -p codesigning | awk '/"Apple Development: / {print $2}')"
  certs="$(mktemp -d)"
  security find-certificate -a -c "Apple Development" -p | awk -v dir="$certs" \
    '/BEGIN CERT/ {n++} {print > (dir "/" n ".pem")}'
  for pem in "$certs"/*.pem(N); do
    sha="$(openssl x509 -in "$pem" -noout -fingerprint -sha1 | sed 's/.*=//; s/://g')"
    if openssl x509 -in "$pem" -noout -subject -nameopt multiline | grep -q "organizationalUnitName *= *$TEAM_ID$" \
        && grep -qx "$sha" <<< "$valid"; then
      IDENTITY="$sha"
      break
    fi
  done
  rm -rf "$certs"
  [[ -n "$IDENTITY" ]] || {
    echo "Нет сертификата «Apple Development» команды $TEAM_ID."
    echo "Xcode → Settings → Apple Accounts → Personal Team → Manage Certificates → + → Apple Development."
    echo "Или UNSIGNED=1 ./build.sh — без подписи."
    exit 1
  }
fi

KDSA_PREFIX="${KDSA_PREFIX:-$BUILD_DIR/deps/kdsingleapplication-$KDSA_TAG}"
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
    -DNEXTCLOUD_FORK_UNSIGNED=ON -DDEVELOPMENT_TEAM="$TEAM_ID" \
    -DBUILD_FILE_PROVIDER_MODULE=ON -DBUILD_UPDATER=OFF -DBUILD_TESTING="${CLIENT_TESTING:-OFF}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
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
rm -rf "$BUILD_DIR/package"   # cmake --install не убирает файлы прошлых сборок
cmake --install "$BUILD_DIR/client" > "$LOG/package.log" 2>&1 || { tail -n 30 "$LOG/package.log"; exit 1; }
APP="$BUILD_DIR/package/Nextcloud Fork.app"
[[ -d "$APP" ]] || { echo "Нет пакета: $APP"; exit 1; }

# cmake --install кладёт плагины VFS уже после macdeployqt, и они ссылаются на Qt из Homebrew:
# переводим их на копии в Contents/Frameworks, иначе в процессе окажутся две Qt.
for plugin in "$APP"/Contents/PlugIns/*.dylib(N); do
  for dep in $(otool -L "$plugin" | tail -n +2 | awk '{print $1}' | grep "^$HB/"); do
    if [[ "$dep" == *.framework/* ]]; then rel="${dep##*/lib/}"; else rel="${dep:t}"; fi
    [[ -e "$APP/Contents/Frameworks/$rel" ]] || { echo "Нет в Contents/Frameworks: $rel (для ${plugin:t})"; exit 1; }
    install_name_tool -change "$dep" "@executable_path/../Frameworks/$rel" "$plugin" 2>/dev/null
  done
  for rpath in $(otool -l "$plugin" | awk '/LC_RPATH/{getline; getline; print $2}' | grep -v "^@"); do
    install_name_tool -delete_rpath "$rpath" "$plugin" 2>/dev/null
  done
  codesign --force --sign - "$plugin" 2>/dev/null
done
# Копии библиотек клиента в Contents/MacOS от cmake --install не используются (грузятся из Frameworks).
rm -f "$APP"/Contents/MacOS/lib*.dylib
# Проверка: ни один файл пакета не грузит библиотеки из Homebrew (собственный ID библиотеки не считается).
brew_linked=()
while IFS= read -r -d '' file; do
  own_id="$(otool -D "$file" 2>/dev/null | tail -n +2)"
  otool -L "$file" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep -vxF "${own_id:-@none}" | grep -q "^$HB/" \
    && brew_linked+=("${file#$APP/}")
done < <(find "$APP" -type f \( -name "*.dylib" -o -perm +111 \) -print0)
if (( ${#brew_linked} )); then
  printf 'В пакете остались ссылки на Homebrew:\n'; printf '  %s\n' "${brew_linked[@]}"
  exit 1
fi

if [[ -n "$IDENTITY" ]]; then
  # Порядок и флаги — как в admin/osx/mac-crafter (Signer.swift): изнутри наружу, внешняя подпись без --deep.
  echo "• Подпись (команда $TEAM_ID)"
  ENT="$BUILD_DIR/client/shell_integration/MacOSX"
  sign() { codesign --force --timestamp --options=runtime --sign "$IDENTITY" "$@" >> "$LOG/sign.log" 2>&1 \
    || { echo "Не подписалось: ${@[-1]}"; tail -n 5 "$LOG/sign.log"; exit 1; } }
  : > "$LOG/sign.log"
  for item in "$APP"/Contents/Library/LoginItems/*.app(N); do
    sign --entitlements "$ENT/FinderSyncBroker.entitlements" "$item"
  done
  for appex in "$APP"/Contents/PlugIns/*.appex(N); do
    for inner in "$appex"/Contents/Frameworks/*(N); do sign "$inner"; done
    sign --entitlements "$ENT/${${appex:t}%.appex}.entitlements" "$appex"
  done
  for framework in "$APP"/Contents/Frameworks/*(N); do sign "$framework"; done
  while IFS= read -r -d '' lib; do sign "$lib"; done \
    < <(find "$APP/Contents" -name "*.dylib" -not -path "*/Frameworks/*" -not -path "*.appex/*" -print0)
  for exe in "$APP"/Contents/MacOS/*(N); do
    [[ "${exe:t}" == "Nextcloud Fork" ]] || sign "$exe"
  done
  sign --entitlements "$BUILD_DIR/client/admin/osx/macosx.entitlements" "$APP"
  codesign --verify --deep --strict "$APP" >> "$LOG/sign.log" 2>&1 || { tail -n 10 "$LOG/sign.log"; exit 1; }
fi

installer="$BUILD_DIR/Nextcloud-Fork-$fork_version-arm64.pkg"
if [[ -z "$IDENTITY" ]]; then
  echo "Собрано без подписи: $APP. PKG требует подписанного приложения."
  exit 0
fi
if [[ -e "$installer" ]]; then
  mv "$installer" "$installer.previous"
fi
python3 admin/fork/make-installer.py --app "$APP" --output "$installer"
python3 admin/fork/publish-installer.py --source "$installer" --directory "$HOME/Github_local/pkg" \
  > "$LOG/publish-installer.json"
echo "Собрано: $APP"
echo "Установщик: $HOME/Github_local/pkg/${installer:t}"
echo "Откройте установщик для установки в /Applications/RomanenkoApp."
