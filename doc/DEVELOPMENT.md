<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Разработка и выпуск

## Политика владельца от03.11.2026

Этот многомодульный проект использует накопление изменений. После команды «собери релиз»03.11 собран0.6.7; последний готовый PKG0.6.8, установленная версия0.6.7. Metadata0.6.8 повышена один раз при подготовке; исправления кандидата не повышают её снова. В обычной разработке сохранять последний подготовленный номер, следующую цель указывать в Unreleased. Обычная просьба добавить функцию не разрешает сборку/упаковку/установку/публикацию. После0.6.9 идёт0.7.0. Установка готового PKG — отдельный шаг владельца.

## Переиспользование среды

Проверено03.11: Xcode27.0/27A266a, CMake4.4.5. Клиент использует существующий Homebrew/Qt6 и Swift Package.resolved; зависимости на этапе локального имени не добавлялись. Не создавать новую среду без причины и не обновлять пакеты автоматически.

На Mac владельца репозиторий ~/Github_local/nextcloud-fork, установленная копия /Applications/RomanenkoApp/Nextcloud Fork.app. build — symlink на ~/Library/Caches/RomanenkoSources/nextcloud-fork/validation-build-20261101; актуальный повторно используемый cache называется fork-0.6.1, хотя последний подготовленный PKG0.6.8. Для компиляции использовать физический BUILD_DIR: логический symlink прежде ломал moc/PCM paths. Сборочные артефакты вне синхронизируемого Github_local. Общая папка доставки PKG — ~/Github_local/pkg, current+previous только этого проекта.

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


## Уточнение QA и выпуск0.6.7 —03.11

Диагностические сборки/тесты в рамках задачи теперь постоянно разрешены владельцем, без ожидания команды выпуска. Релиз/публикация/изменение installed app по прежнему отдельные действия. По завершении диагностик закрыть процессы и удалить собственные временные PNG/raw logs/expanded PKG/отдельные стенды; reusable cache и входные зависимости не сносить. Для0.6.7 ConnectionNameEditorTest с новым moc выполнен в isolated .tests bundle;10 passed/1skip interactive preview, русские offscreen default/draft/invalid действительно просмотрены. Сборка build.sh — consuming check вместо legacy NextcloudDev с product-path=/Applications.


Проверка автозапуска postinstall (Unreleased0.6.8): `python3 test/hybrid/test_installer_launch.py`, изолированные commands, без запуска настоящего клиента. В следующем configure зарегистрируется ForkInstallerLaunch. Реальную root installation и следующий release PKG выполнять отдельно по команде владельца.


## Diagnostic master port —07.11.2026

Root configure использует тот же physical validation-build-20261101/fork-0.6.1/client и fork/hybrid/unsigned/test-isolation flags. Native consuming target all и schema desktopclient успешно собраны без установки. Package tests: из shell_integration/MacOSX/NextcloudFileProviderKit запускать swift test --no-parallel --jobs4 --scratch-path <physical>/swift-tests --config-path <physical>/swift-test-config; existingPackage.resolved/checkouts переиспользуются, реальные database directories не передаются. Фактические versions/revisions записаны в CURRENT_STATE/THIRD_PARTY. Root workspace Package.resolved больше не tracked upstream, не возвращать его старую35.0.1 версию автоматически.

Qt redirect test requires Homebrew qthttpserver6.12.0_1 (совпадает с qtbase6.12.0_2/qtwebsockets6.12.0_1), теперь проверяется build.sh при CLIENT_TESTING=ON. Проверить HAVE_QHTTPSERVER=1 в compile_commands И наличие testDirectUrlCredentials в MOC/Qt test functions: после установки модуля один reconfigure без regeneration moc оказался недостаточен. Удалять весь build не нужно; обновить source mtime/сгенерировать только NextcloudPropagatorTest moc, пересобрать target и запустить explicit testDirectUrlCredentials.

Перед будущей0.6.8 установкой отдельно подготовить актуальный metadata backup/rollback для Realm→SQLite. Диагностический build не является разрешением этой миграции; production bundles не запускать с пустыми settings.


После master QA запущенные test apps закрыты; запускных .app/.appex/.xctest в диагностическом cache больше нет. Compiler objects/package repositories оставлены для reuse. Перед новым прогоном выполнить configure и build/relink, не ссылаться на удалённый test bundle и не применять --skip-build из прежнего отчёта. Metadata0.6.7 остаётся до отдельного выпуска0.6.8; build.sh обычного релиза без bump нельзя запускать поверх immutable PKG0.6.7.


07.11: подготовлен PKG0.6.8, без установки. Подпись source/payload valid, versions35.0.50.68/68. Before installing: metadata backup обязателен как согласованный следующий этап из-за Realm→GRDB и автоматического запуска postinstall. Последние checksum/размер/результаты в CURRENT_STATE.
