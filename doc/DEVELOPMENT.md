<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Разработка и выпуск

## Политика владельца от03.11.2026

Этот многомодульный проект использует накопление изменений. После команды «собери релиз»03.11 собран0.6.6; последний готовый PKG0.6.6, установленная версия0.6.4. Metadata0.6.6 повышена один раз при подготовке; исправления кандидата не повышают её снова. В обычной разработке сохранять последний подготовленный номер, следующую цель указывать в Unreleased. Обычная просьба добавить функцию не разрешает сборку/упаковку/установку/публикацию. После0.6.9 идёт0.7.0. Установка готового PKG — отдельный шаг владельца.

## Переиспользование среды

Проверено03.11: Xcode27.0/27A266a, CMake4.4.5. Клиент использует существующий Homebrew/Qt6 и Swift Package.resolved; зависимости на этапе локального имени не добавлялись. Не создавать новую среду без причины и не обновлять пакеты автоматически.

На Mac владельца репозиторий ~/Github_local/nextcloud-fork, установленная копия /Applications/RomanenkoApp/Nextcloud Fork.app. build — symlink на ~/Library/Caches/RomanenkoSources/nextcloud-fork/validation-build-20261101; актуальный повторно используемый cache называется fork-0.6.1, хотя последний подготовленный PKG0.6.6. Для компиляции использовать физический BUILD_DIR: логический symlink прежде ломал moc/PCM paths. Сборочные артефакты вне синхронизируемого Github_local. Общая папка доставки PKG — ~/Github_local/pkg, current+previous только этого проекта.

## Проверки без сборки

`git diff --check`, ревью source diff, clang-format затронутых диапазонов, статический clang-tidy по существующей compilation database. Для новых translation units используется отдельная временная копия compile_commands с унаследованными флагами соседней цели; она не заменяет реальную конфигурацию/компиляцию. Не запускать старые тестовые binaries как проверку нового кода.

## После явной команды на проверки/выпуск

Профиль: cmake -S test/hybrid -B <physical-build>/policy -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew; cmake --build <physical-build>/policy; ctest --test-dir <physical-build>/policy --output-on-failure.

Новые тесты: AccountTest, AccountManagerTest, ConnectionNameEditorTest, DomainDisplayNameReconcilerTest, NativeDomainMutationQueueTest, ForkDomainDisplayNameTest. После настоящего configure проверить изолированные bundle IDs, затем узкий CTest по этим именам. GUI проверить с fake account, Save/close/reset, затем на отдельном тестовом File Provider account с неизменностью domain UUID и данных.

Для изменений src обязателен xcodebuild build -target NextcloudDev в NextcloudIntegration; для интеграции — xcodebuild build -scheme desktopclient. Известен official team guard NextcloudDev (не называть успешным, если вновь сработает). Изменения Swift package требуют swift test; последний полный Swift run0.6.3 использовал --no-parallel из-за известной гонки общей лог-конфигурации тестов.

Полная ранее проверенная команда: BUILD_DIR=<physical-cache>/fork-0.6.1 KDSA_PREFIX=<cache>/deps/kdsingleapplication-v1.2.1 CLIENT_TESTING=ON ./build.sh. Она создаёт и автоматически копирует PKG в общий каталог, поэтому не запускать её в обычной разработке. Установка отдельно через native Installer; auth при необходимости вводит владелец. Перед выпуском проверить signatures source/payload/installed, версию, Finder label и сохранение domain ID. Не патчить подписанный bundle на месте.

Откат приложения — предыдущий PKG после оценки совместимости данных; откат миграций базы не обещан. При переносе на другой Mac владелец отдельно настраивает подпись/разрешения/авторизацию: секреты не входят в Git или инструкции.


## Установщик: автоматическое завершение0.6.6

Preinstall запрашивает graceful exit установленного клиента через его --quit от имени console user и ждёт до30s. Механизм проверяется без реального клиента командой `python3 test/hybrid/test_installer_quit.py` (shell с изолированными mocks). Профиль CTest ForkInstallerQuit включает те же5 тестов. Эта проверка не требует сборки приложения. Реальная установка от root проверяется отдельно после подготовки PKG и команды владельца.
