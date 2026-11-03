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

# # До03.11.2026

Проверяемая подробная история0.6.0–0.6.4 сохранена в FORK.md (пользовательские изменения), CURRENT_STATE и PROJECT_HANDOFF (сборки, тесты, инциденты и ограничения). Не дублировать весь журнал сюда и не выдавать старые результаты за проверки нового кода.


### Завершение исходного среза03.11.2026

Для собственного процесса добавлена общая `NativeDomainMutationQueue` (новые nativedomainmutationqueue.{h,cpp} и testnativedomainmutationqueue.cpp): fork create/remove/rename сериализованы до реального native callback. Rename повторно читает регистрацию **внутри** очереди, поэтому удалённый предыдущей операцией ID не добавляется заново; writable metadata берутся из свежего объекта. Освобождение очереди не зависит от Qt owner thread, иначе старые синхронные wrappers могли бы заблокироваться. После caller timeout уже отправленная mutation остаётся fenced; ещё не стартовавшие legacy add/remove помечаются Expired и не отправляются позднее. Поздние callback используют heap/strong captures, cleanup не держит уничтоженный MacImplementation. Для fork waits ограничены10s; official профиль сохраняет прямые вызовы и прежний режим ожидания.

Добавлены семь исходных regression scenarios очереди: оба порядка rename/remove, fresh snapshot, callback fence, query failure, duplicate callbacks, completion с worker без Qt event loop, жизнь операции после уничтожения wrapper. Они **не запускались**. Это реализованная защита по исходникам, не доказательство runtime отсутствия гонок. External-process removal vs OS addDomain без CAS всё ещё вне гарантии; результат уже начавшегося native create/remove после timeout неизвестен. Эти границы проверить перед выпуском; не удалять реальные domains ради теста.

Финальные статические проверки: новая queue.cpp прошла два clang-tidy -fix прохода exit0/no changes. Native fileproviderdomainmanager.mm после исправления обнаруженной синтаксической ошибки enum прошёл ещё два прохода exit0/no changes. Новые editor/reconciler/domain-name production units и model test sources уже проверены выше. Для трёх новых Qt test sources (editor/reconciler/queue) полная static проверка блокируется отсутствующими generated .moc; ошибок до этих include в последнем анализе не осталось, но не считать это compile pass. Все затронутые C++ files форматированы, `git diff --check` успешен; индекс локальных documentation links проверен — broken links0. Локальные логи только /private/tmp/nextcloud-connection-name-static, в Git сырые логи не добавляются.

Готовность: исходники функции подготовлены, выпуск **не готов к утверждению** без compiled tests и GUI/native validation. Метаданные и установленная копия по-прежнему0.6.4. Новый PKG не создан. Следующее действие после отдельного разрешения на проверки — конфигурация и узкие Account/AccountManager/ConnectionNameEditor/DomainDisplayNameReconciler/NativeDomainMutationQueue/ForkDomainDisplayName tests, затем native workflow на отдельном тестовом аккаунте (rename/reset/restart/failure/concurrent removal) с UUID и data preservation. До команды на выпуск номер0.6.5 остаётся только целевым Unreleased.
