<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Источники и лицензии

Форк основан на [Nextcloud Desktop](https://github.com/nextcloud/desktop), авторы Nextcloud GmbH, Nextcloud contributors и исторические ownCloud contributors указаны в исходных SPDX/copyright headers. Последняя установленная база —35.0.1; текущие исходники Unreleased0.6.8 перенесены с master cc930c7ce5703c37c40cd1b8dc84e1c6c9f1d119 относительно ad4a8503bcbb1749ea4f5955a65688cbe32305df; метаданные Git snapshot не заменяют историю upstream. Локальные изменения форка, включая pending connectionName, описаны в FORK.md/CHANGELOG/DEVELOPMENT_LOG.

Сохранены COPYING и LICENSES/, REUSE metadata и per-file SPDX. Они являются источником применимых условий; этот индекс не меняет лицензию чужих файлов. Основной код GPL-2.0-or-later; часть существующих тестов имеет собственные CC0 headers, их не заменять. Новые файлы этого среза имеют GPL-2.0-or-later headers по правилам проекта.

Сторонние компоненты src/3rdparty имеют собственные headers и документы; CMake scripts также содержат cmake/modules/COPYING-CMAKE-SCRIPTS. Swift dependencies/версии определяются Package.swift/Package.resolved; не представлять все зависимости как одну лицензию проекта. Полный dependency/license audit перед распространением остаётся необходимым, этот документ не объявляет его выполненным.

Новых внешних зависимостей и заимствованных фрагментов кода для local connectionName не добавлено. Работа использует существующие Account/QSettings/Qt Widgets и public NSFileProviderManager APIs; Apple API declarations не копировались как реализация. Ранее исследованный недокументированный naming plist key описан отдельно с доказательствами, без включения реализации Apple в исходники.

При коммите AI-assisted content требуется Assisted-by с фактическим именем/моделью агента; Signed-off-by добавляет только человек. Текущий срез выполнялся Codex и делегированными GPT-6.1 Sol агентами; это не copyright attribution внешнему разработчику. Репозиторий origin — приватный sawabeckham/nextcloud-fork; публичных PR/issue/комментариев от имени владельца не отправлялось.


Установщик0.6.6: новое graceful-quit поведение и тесты написаны в проекте, внешние фрагменты/зависимости не добавлены. Использованы существующий CLI Nextcloud Application::parseOptions(--quit), системные launchctl/sudo/pgrep/stat/id/sleep; прежние атрибуции Nextcloud/ownCloud в installer scripts сохранены. Новые test sources GPL-2.0-or-later/SPDX.


## Полный source import master —07.11.2026

Официальный источник https://github.com/nextcloud/desktop/commit/cc930c7ce5703c37c40cd1b8dc84e1c6c9f1d119; diff относительно ad4a8503bcbb1749ea4f5955a65688cbe32305df (v35.0.1). Авторство сохранено per-file SPDX/copyright и в официальной истории; все incoming файлы происходят из upstream, не объявляются нашей оригинальной реализацией. Local adaptations: exact-ID auxiliary guard/root label overlay в Extension/FileProviderExtension.swift и Item/Item.swift; native queue/display names в fileproviderdomainmanager/settingscontroller и swizzled FileProviderXPCUtils tests. Protected AGENTS и Transifex translations не заменяются; это явные исключения к полному импорту.

Новая upstream зависимость GRDB.swift (https://github.com/groue/GRDB.swift, MIT, copyright Gwendal Roué и участники — точный текст проверять в license зависимостей). Package.swift требует from7.11.0; реально проверен7.12.0/rev92a554cd0d5e8d67cb18993c6054e73fe42c066c. Локальный checkout LICENSE проверен: Copyright(C)2015–2025 Gwendal Roué, стандартные MIT terms. NextcloudKit повышен upstream до from7.6.3. Фактически RealmSwift20.0.6/realm-core20.1.5; RealmSwift from20.0.5 ещё нужен для legacy import, прежняя лицензия сохраняется. Новый полный dependency/license audit перед распространением не заявляется выполненным. Код cancellation/scanning/GRDB и соответствующие тесты сохраняют upstream GPL headers, существующие COPYING/LICENSES не удаляются.
