<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Журнал разработки форка

## 03.11.2026 — Unreleased0.6.5: локальное имя подключения

Задача: пользователь задаёт собственное название подключения; та же подпись должна появляться в Finder. Последний выпуск и установленная копия остаются0.6.4. Изменены Account/AccountManager (поле, валидация, INI), добавлен ConnectionNameEditor в Connection settings, native selector и async DomainDisplayNameReconciler с тестовыми seams. Старые identity/DAV/URL не меняют смысла.

Отдельный локальный коммит этого этапа фиксирует только модель Account/сохранение INI и их regression tests. UI/native-срез пока остаётся незавершённым в рабочем дереве; прежний большой diff0.6.4 не включается автоматически в этот коммит.

Источники тестов дополнены: модель, persistence, standalone QWidget, выбор имени и async lifecycle. AccountSettingsTest исключён при BUILD_FILE_PROVIDER_MODULE старым upstream guard, поэтому для нового редактора создан отдельный ConnectionNameEditorTest, доступный в File Provider build. Компиляция/запуск этих тестов, полная сборка и реальный Finder отсрочены по политике выпуска. Результаты статических проверок и незавершённые границы — последний CURRENT_STATE/PROJECT_HANDOFF.

Подтверждённое окружение переиспользовано; dependency updates/новый PKG/установка не выполнялись. Native concurrent removal vs uncancellable addDomain — обязательная проверка до выпуска; непроверенное не считать готовым. Следующий шаг — ревью накопленного среза и разрешённые проверки, затем после команды владельца выпускать целевую версию.

## До03.11.2026

Проверяемая подробная история0.6.0–0.6.4 сохранена в FORK.md (пользовательские изменения), CURRENT_STATE и PROJECT_HANDOFF (сборки, тесты, инциденты и ограничения). Не дублировать весь журнал сюда и не выдавать старые результаты за проверки нового кода.


### Завершение исходного среза03.11.2026

Для собственного процесса добавлена общая `NativeDomainMutationQueue` (новые nativedomainmutationqueue.{h,cpp} и testnativedomainmutationqueue.cpp): fork create/remove/rename сериализованы до реального native callback. Rename повторно читает регистрацию **внутри** очереди, поэтому удалённый предыдущей операцией ID не добавляется заново; writable metadata берутся из свежего объекта. Освобождение очереди не зависит от Qt owner thread, иначе старые синхронные wrappers могли бы заблокироваться. После caller timeout уже отправленная mutation остаётся fenced; ещё не стартовавшие legacy add/remove помечаются Expired и не отправляются позднее. Поздние callback используют heap/strong captures, cleanup не держит уничтоженный MacImplementation. Для fork waits ограничены10s; official профиль сохраняет прямые вызовы и прежний режим ожидания.

Добавлены семь исходных regression scenarios очереди: оба порядка rename/remove, fresh snapshot, callback fence, query failure, duplicate callbacks, completion с worker без Qt event loop, жизнь операции после уничтожения wrapper. Они **не запускались**. Это реализованная защита по исходникам, не доказательство runtime отсутствия гонок. External-process removal vs OS addDomain без CAS всё ещё вне гарантии; результат уже начавшегося native create/remove после timeout неизвестен. Эти границы проверить перед выпуском; не удалять реальные domains ради теста.

Финальные статические проверки: новая queue.cpp прошла два clang-tidy -fix прохода exit0/no changes. Native fileproviderdomainmanager.mm после исправления обнаруженной синтаксической ошибки enum прошёл ещё два прохода exit0/no changes. Новые editor/reconciler/domain-name production units и model test sources уже проверены выше. Для трёх новых Qt test sources (editor/reconciler/queue) полная static проверка блокируется отсутствующими generated .moc; ошибок до этих include в последнем анализе не осталось, но не считать это compile pass. Все затронутые C++ files форматированы, `git diff --check` успешен; индекс локальных documentation links проверен — broken links0. Локальные логи только /private/tmp/nextcloud-connection-name-static, в Git сырые логи не добавляются.

Готовность: исходники функции подготовлены, выпуск **не готов к утверждению** без compiled tests и GUI/native validation. Метаданные и установленная копия по-прежнему0.6.4. Новый PKG не создан. Следующее действие после отдельного разрешения на проверки — конфигурация и узкие Account/AccountManager/ConnectionNameEditor/DomainDisplayNameReconciler/NativeDomainMutationQueue/ForkDomainDisplayName tests, затем native workflow на отдельном тестовом аккаунте (rename/reset/restart/failure/concurrent removal) с UUID и data preservation. До команды на выпуск номер0.6.5 остаётся только целевым Unreleased.


Исходники редактора имени, injected reconciler/mutation queue и их test sources сохраняются отдельным локальным коммитом компонентов. Native wiring в fileproviderdomainmanager.{h,mm}, AccountSettings/CMake integration и прежний большой diff0.6.4 остаются в рабочем дереве; их нельзя потерять через reset/clean. Эти исходники всё ещё не собраны. Новые standalone test .moc отсутствуют до разрешённой генерации, ни один test runtime pass для этого этапа не заявляется.


## 03.11.2026 — начата подготовка релиза0.6.5

Владелец прямо написал «готовим релиз». Номер FORK_VERSION.cmake повышен один раз с0.6.4 до0.6.5; исправления этого кандидата не требуют нового bump. Разрешены сборки, compiled tests и подготовка PKG в согласованной общей папке. Установка/production не входят в эту новую команду: установленная0.6.4 остаётся без изменений. Переиспользуется physical cache validation-build-20261101/fork-0.6.1; сначала configure/build/test без упаковки, затем подписанный PKG только после необходимых проверок. Код0.6.5 пока не считать проверенным/выпущенным. Native safety/UI review делегированы независимо, reviewers не изменяют файлы во время сборки.


## 03.11.2026 — проверки кандидата0.6.5 перед упаковкой

После команды «готовим релиз» выполнен настоящий configure и полная ARM64 CMake build(all), включая main product и Finder/File Provider extensions: exit0. Новые Qt test bundles проверены до запуска:116 IDs имеют отдельный site.manro.nextcloudfork.tests.* namespace. Узкие шесть целей прошли6/6 после исправлений; весь CTest118/118 PASS (48.92s), профиль11/11 PASS. Это результаты0.6.5, а не повторное цитирование0.6.4. Полный Swift suite не повторялся: Swift source delta0.6.5 отсутствует; последняя полная Swift проверка относится к0.6.3. Новые расширения действительно скомпилированы полной сборкой.

Release review нашёл blocker новых caller timeouts: AccountManager уже удаляет settings/credentials до native remove signal; Expired мог навсегда пропустить queued removal, а поздний add оставлял UUID без mapping. Исправлено: fork create/remove используют NativeDomainMutationQueue.enqueueAndWait с independent promise/future, ждут реальный callback; Expired/CAS/timeouts для legacy wrappers удалены. Official direct wait contract сохранён. Добавлены regression scenarios задержанного create success/error с mapping и удаления за rename с recovery path; новые queue cases прошли в CTest. Известное ограничение остаётся прежним: legacy create/remove могут ждать macOS без ограничения; асинхронный account lifecycle — отдельный этап. Новый rename сообщает timeout, но не снимает mutation fence.

При первой компиляции обнаружены duplicate MOC symbols: test targets повторно включали cpp уже содержащегося в nextcloudCore QObject. Лишние target_sources удалены для Auxiliary/DomainName/NativeQueue tests; тестируется production archive. Первый test run выявил package validation: preferred UTType(.app) оказался flat application-file, а не application-bundle. Исправление перечисляет directory types суффикса, проверяет package среди них; тот же helper используется для hostname fallback. Реальный Swift probe и compiled tests подтвердили .app/.bundle/iWork packages, uppercase и leading-dot cases. После исправления6/6 и118/118 прошли. Номер кандидата остаётся0.6.5.

Визуальная проверка: синтетический QApplication preview выполнился60s и завершился успешно, но CUA backend не ответил (timeout457s), поэтому реальный экран не наблюдался. Вместо этого Qt offscreen fixture с invalid URL отрисовал7 собственных состояний в ui-0.6.5 под physical build cache: default, Cyrillic draft/saved, invalid, reset draft/saved, Finder error. Render test3/3 PASS,4 репрезентативных PNG действительно просмотрены: читаемость/переносы/кнопки без перекрытия. Это проверка Qt-компонента и mock account; реальный Finder/OS domain rename не проверялись и не должны называться validated. Никакие live accounts или registrations в preview/render не используются.

Финальный scoped clang-format и два run-clang-tidy -p build -header-filter='.*' -config-file .clang-tidy -fix для release fixes и новых tests прошли exit0/no changes, теперь moc generated настоящей сборкой. Логи статического анализа /private/tmp/nextcloud-release-0.6.5; build/test/render logs — physical cache fork-0.6.1, сырые логи не входят в Git. Машинные SwiftPM mirrors и Python __pycache__ исключены .gitignore, не удалены из среды.

Legacy xcodebuild -target NextcloudDev не запускается для этого кандидата: NextcloudDev/Craft.sh жёстко задаёт --product-path=/Applications и может заменять installed app, что не разрешено командой подготовки релиза. Main product проверен реальной consuming ARM64 build(all); невозможность безопасно выполнить этот альтернативный legacy target отмечена явно. Установленная0.6.4 остаётся без изменений. Полная hybrid-синхронизация и старые recovery/NAS reconciliations по-прежнему вне этой проверки. Upstream35.0.2 не переносился в этот feature slice.

Следующий шаг: coherent source snapshot прежней рабочей базы0.6.4 +0.6.5 candidate, затем финальная подпись/PKG/проверка payload и доставка в согласованный ~/Github_local/pkg. Production installation и live Finder smoke требуют отдельной команды; не запускать staged production bundle с пустым --confdir, иначе тот же bundle ID может увидеть реальные domains как orphan.


## 03.11.2026 — PKG 0.6.5 подготовлен, установка не выполнялась

По команде владельца «готовим релиз» подготовлен ARM64 PKG0.6.5 на основе upstream35.0.1. Исходный снимок приложения — `97c9e25`; последующие изменения этого этапа только документируют результат и не требуют пересборки. Полная сборка `CLIENT_TESTING=ON ./build.sh` завершилась exit0, подписанное приложение находится в physical cache `validation-build-20261101/fork-0.6.1/package/Nextcloud Fork.app`. Использован существующий cache/KDSingleApplication, без перенастройки среды. Main short version `35.0.1fork0.6.5 (build 65)`, CFBundleVersion35.0.1.65; все три расширения build65. Идентификаторы и app name сохранены.

Готовый артефакт: `~/Github_local/pkg/Nextcloud-Fork-0.6.5-arm64.pkg`, 75 239 734 байта, SHA-256 `cc4187abddb213ea5378a8ca8597ae1f9c50b8f3491c2ea3d978d4e5ddd478e7`. Общая папка содержит0.6.5 и0.6.4; SHA-256 предыдущего0.6.4 не изменился. Копия0.6.5 в cache побайтно совпадает с общей папкой. `pkgutil --expand-full` выполнен в отдельный временный каталог; исходный и payload бандлы прошли `codesign --verify --deep --strict`, TeamIdentifier EW76LLFDSH, main executable arm64. Побайтно совпадают executable, Info.plist, CodeResources и три extension Info.plist. Payload install-location `/Applications/RomanenkoApp`, version0.6.5; NSFileProviderHideExtensionName — настоящий Boolean true. Подпись Apple Development приложения действующая; внешний PKG не подписан Developer ID Installer и не нотариализован. Это локальный артефакт для согласованного Mac, не подтверждение распространения на других Mac.

Проверки: CTest118/118 PASS; профиль11/11 PASS (повторён build.sh перед упаковкой); isolated renderer3/3 PASS и7PNG, четыре состояния действительно просмотрены. Scoped clang-format и два реальных run-clang-tidy с generated moc — exit0, no changes. `git diff --check` прошёл. Первые ошибки duplicate MOC/package UTType и исправление native lifecycle описаны в DEVELOPMENT_LOG; не повторять старые реализации. GUI backend timeout не позволяет заявить визуальную проверку нативного окна; offscreen PNG проверяют только редактор. Legacy NextcloudDev target не запускался из-за записи в /Applications; consuming CMake main/extensions build(all) успешна. Неизменённый Swift suite не повторялся.

Установка/запуск нового production bundle не выполнялись. Проверены installed Info.plist и receipt: остаются0.6.4/35.0.1.64. Read-only fileproviderctl после тестов подтвердил существующий real domain и provider bundle из установленного RomanenkoApp; никакие live domains/accounts не менялись. Реальное новое имя в Finder, rename/reset/restart, native failure и concurrent account removal на тестовом аккаунте остаются непроверенными. Сохранность всей синхронизации/NAS и старой preserved папки не выводится из этих тестов. Старые recovery данные не трогать. Upstream35.0.2 в этот feature release не переносился.

Следующий шаг — только по команде владельца установить этот PKG и проверить редактор/имя Finder на согласованном аккаунте. Не запускать staging production bundle с пустым --confdir; не пересоздавать домен ради подписи. Legacy create/remove по прежнему контракту ждут реального системного callback без timeout; async account lifecycle — отдельный этап. Асинхронный rename имеет timeout уведомления, но сохраняет mutation fence до callback; внешний процесс вне очереди остаётся системной границей проверки. Публичные release/тег/PR и binaries в GitHub не создавались. Исходники и результаты сохраняются в согласованной приватной ветке origin/fork; точный documentation commit сверять через git log.
