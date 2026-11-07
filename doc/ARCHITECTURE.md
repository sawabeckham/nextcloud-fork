<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Устройство форка

Текущий перенос исходников — Nextcloud Desktop master cc930c7c (35.1.0 alpha), Unreleased форка0.6.8. Установленная0.6.7 остаётся на35.0.1. C++/Qt: src/common (политики), src/libsync/src/csync (синхронизация), src/gui (аккаунты/UI). Swift File Provider — shell_integration/MacOSX/NextcloudFileProviderKit; extension bundles — NextcloudIntegration. build.sh управляет общей сборкой, подписью и PKG.

Hybrid сохраняет classic папки при включённом File Provider; запреты на Library/CloudStorage, пересечения и симлинки находятся в hybridsyncpaths. Реальный журнал и сетевой/серверный URL не являются пользовательскими подписями.

## Локальное имя подключения

Account.connectionName — отдельное локальное поле, по умолчанию пустое. AccountManager сохраняет его в Accounts/<account-id>/connectionName. Старый displayName хранит DAV-имя пользователя и не меняет значения. User ID, UUID, URL, credentials и пути classic sync не переименовываются.

AccountSettings открывает ConnectionNameEditor над NetworkSettings в диалоге Connection settings. Draft не сохраняется до Save name; очистка поля и сохранение возвращают автоматическое имя. Валидация модели ограничивает длину/разделители/управляющие символы, macOS дополнительно отклоняет package-суффиксы через UTType.

FileProviderDomainManager выбирает custom name или прежний host[:port]/безопасный fallback. DomainDisplayNameReconciler получает инъецируемые query/update/target callbacks, сравнивает owner и domain ID, последовательно выполняет только обновления существующих mapped registrations. Pending native mutation после timeout остаётся ограждённой до callback. Служебный helper исключён. Изменение имени вызывает reconciliation; старт также восстанавливает сохранённое имя.

Apple displayName readonly: native backend повторно вызывает addDomain с тем же ID и переносит writable properties. Внешние тома пропускаются. NativeDomainMutationQueue последовательно исполняет fork create/remove/rename и повторно проверяет наличие домена внутри fence перед rename. Завершение native callback не зависит от Qt event loop; rename timeout не снимает fence уже начавшейся операции; legacy create/remove ждут настоящий callback через enqueueAndWait, не теряя mapping/удаление по caller timeout. OS API не предоставляет conditional-update/cancel: внешнее конкурентное удаление и поздний результат уже отправленной операции остаются границами; native lifecycle ещё требует реальной проверки перед выпуском. Дополнительная безопасность и тесты описаны в handoff; это исходники в разработке, native контракт проверяется изолированно; live migration/rename ещё требуют отдельной проверки.

## Системные ограничения

Hostname-only0.6.4 использует недокументированный NSExtension.NSFileProviderHideExtensionName; подтверждено только на текущем Mac. File Provider domain сохраняется после выхода main app. Удаление регистрации — отдельная операция с данными, не способ закрытия приложения. Тестовые .app имеют отдельные bundle IDs; старые тестовые binaries до исправления identity не запускать.


## База File Provider после переноса master

FilesDatabaseManager использует GRDB DatabasePool/SQLite WAL; записи и schema находятся в Database/Records и Database/Schema. Подключение к базе не реентерабельно: public методы открывают один access и передают Database внутренним workers. Realm остаётся только для одноразового legacy import (шесть таблиц, включая chunks/cleanup/change-delivery acknowledgements). RealmStoreImporter импортирует в staging transaction и атомарно заменяет SQLite; recoverable open/write failure не публикует пустую базу и повторяется позже. Нечитаемая Realm откладывается как .import-failed, после чего создаётся fresh store. Это важно отдельно проверить перед живым запуском.

Успешный import удаляет Realm. Legacy клиент при откате может создать Realm заново; новый importer считает её более свежим состоянием и заменяет SQLite. Поэтому rollback app bundle сам по себе не является корректным rollback metadata. Ни одна реальная база в этапе переноса не открывается новым production bundle. Исходные Realm/schema и migration tests заимствованы из официального master, детали происхождения в THIRD_PARTY.

Master добавляет cancellation propagation и защиту superseded downloads, depth-batched concurrent working-set reads, coalescer уведомлений ancestors и pin-aware scan. Fork сохраняет auxiliary early-return до database/network init и root label metadata overlay; helper никогда не должен инициировать import. Native domain API использует optional removal result и discovery-success из master, но fork mutations сохраняют очередь до фактического callback, чтобы caller timeout не терял mapping/удаление.
