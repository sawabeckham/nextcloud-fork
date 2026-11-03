<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Источники и лицензии

Форк основан на [Nextcloud Desktop](https://github.com/nextcloud/desktop), авторы Nextcloud GmbH, Nextcloud contributors и исторические ownCloud contributors указаны в исходных SPDX/copyright headers. Текущая перенесённая основа — v35.0.1, upstream commit ad4a8503bcbb1749ea4f5955a65688cbe32305df; метаданные Git snapshot не заменяют историю upstream. Локальные изменения форка, включая pending connectionName, описаны в FORK.md/CHANGELOG/DEVELOPMENT_LOG.

Сохранены COPYING и LICENSES/, REUSE metadata и per-file SPDX. Они являются источником применимых условий; этот индекс не меняет лицензию чужих файлов. Основной код GPL-2.0-or-later; часть существующих тестов имеет собственные CC0 headers, их не заменять. Новые файлы этого среза имеют GPL-2.0-or-later headers по правилам проекта.

Сторонние компоненты src/3rdparty имеют собственные headers и документы; CMake scripts также содержат cmake/modules/COPYING-CMAKE-SCRIPTS. Swift dependencies/версии определяются Package.swift/Package.resolved; не представлять все зависимости как одну лицензию проекта. Полный dependency/license audit перед распространением остаётся необходимым, этот документ не объявляет его выполненным.

Новых внешних зависимостей и заимствованных фрагментов кода для local connectionName не добавлено. Работа использует существующие Account/QSettings/Qt Widgets и public NSFileProviderManager APIs; Apple API declarations не копировались как реализация. Ранее исследованный недокументированный naming plist key описан отдельно с доказательствами, без включения реализации Apple в исходники.

При коммите AI-assisted content требуется Assisted-by с фактическим именем/моделью агента; Signed-off-by добавляет только человек. Текущий срез выполнялся Codex и делегированными GPT-6.1 Sol агентами; это не copyright attribution внешнему разработчику. Репозиторий origin — приватный sawabeckham/nextcloud-fork; публичных PR/issue/комментариев от имени владельца не отправлялось.


Установщик0.6.6: новое graceful-quit поведение и тесты написаны в проекте, внешние фрагменты/зависимости не добавлены. Использованы существующий CLI Nextcloud Application::parseOptions(--quit), системные launchctl/sudo/pgrep/stat/id/sleep; прежние атрибуции Nextcloud/ownCloud в installer scripts сохранены. Новые test sources GPL-2.0-or-later/SPDX.
