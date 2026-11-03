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
