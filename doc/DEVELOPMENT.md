<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Разработка и выпуск

# # Политика владельца от03.11.2026

Этот многомодульный проект использует накопление изменений. Последний выпущенный номер0.6.4 сохраняется в FORK_VERSION.cmake до явной команды готовить релиз. Целевая0.6.5 — Unreleased, не установленная версия. Команда добавить функцию сама по себе не разрешает сборку, упаковку, установку или публикацию. Обязательные compiled checks на этом этапе явно отложены; их нельзя считать успешными. Повторные исправления кандидата одного выпуска не повышают номер снова. После0.6.9 идёт0.7.0.

# # Переиспользование среды

Проверено03.11: Xcode27.0/27A266a, CMake4.4.5. Клиент использует существующий Homebrew/Qt6 и Swift Package.resolved; зависимости на этапе локального имени не добавлялись. Не создавать новую среду без причины и не обновлять пакеты автоматически.

На Mac владельца репозиторий ~/Github_local/nextcloud-fork, установленная копия /Applications/RomanenkoApp/Nextcloud Fork.app. build — symlink на ~/Library/Caches/RomanenkoSources/nextcloud-fork/validation-build-20261101; актуальный повторно используемый cache называется fork-0.6.1, хотя последний собранный релиз0.6.4. Для компиляции использовать физический BUILD_DIR: логический symlink прежде ломал moc/PCM paths. Сборочные артефакты вне синхронизируемого Github_local. Общая папка доставки PKG — ~/Github_local/pkg, current+previous только этого проекта.

# # Проверки без сборки

`git diff --check`, ревью source diff, clang-format затронутых диапазонов, статический clang-tidy по существующей compilation database. Для новых translation units используется отдельная временная копия compile_commands с унаследованными флагами соседней цели; она не заменяет реальную конфигурацию/компиляцию. Не запускать старые тестовые binaries как проверку нового кода.

# # После явной команды на проверки/выпуск

Профиль: cmake -S test/hybrid -B <physical-build>/policy -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew; cmake --build <physical-build>/policy; ctest --test-dir <physical-build>/policy --output-on-failure.

Новые тесты: AccountTest, AccountManagerTest, ConnectionNameEditorTest, DomainDisplayNameReconcilerTest, NativeDomainMutationQueueTest, ForkDomainDisplayNameTest. После настоящего configure проверить изолированные bundle IDs, затем узкий CTest по этим именам. GUI проверить с fake account, Save/close/reset, затем на отдельном тестовом File Provider account с неизменностью domain UUID и данных.

Для изменений src обязателен xcodebuild build -target NextcloudDev в NextcloudIntegration; для интеграции — xcodebuild build -scheme desktopclient. Известен official team guard NextcloudDev (не называть успешным, если вновь сработает). Изменения Swift package требуют swift test; последний полный Swift run0.6.3 использовал --no-parallel из-за известной гонки общей лог-конфигурации тестов.

Полная ранее проверенная команда: BUILD_DIR=<physical-cache>/fork-0.6.1 KDSA_PREFIX=<cache>/deps/kdsingleapplication-v1.2.1 CLIENT_TESTING=ON ./build.sh. Она создаёт и автоматически копирует PKG в общий каталог, поэтому не запускать её в обычной разработке. Установка отдельно через native Installer; auth при необходимости вводит владелец. Перед выпуском проверить signatures source/payload/installed, версию, Finder label и сохранение domain ID. Не патчить подписанный bundle на месте.

Откат приложения — предыдущий PKG после оценки совместимости данных; откат миграций базы не обещан. При переносе на другой Mac владелец отдельно настраивает подпись/разрешения/авторизацию: секреты не входят в Git или инструкции.
