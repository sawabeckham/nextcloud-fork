<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Решения

## 03.11.2026: накопление изменений до команды выпуска

Nextcloud Fork — крупный продукт. Сохранять последний released номер0.6.4 в metadata; целевой0.6.5 вести как Unreleased. Сборка и деплой только по прямой команде владельца. Это заменяет старые указания поднимать версию на каждую правку; прежние версии не переименовывать.

## 03.11.2026: имя подключения отдельно от серверной идентичности

Пользователь хочет своё название сервера и ту же подпись в Finder. Выбран локальный per-account connectionName; пустое значение означает прежний адрес. DAV displayName — имя пользователя сервера, а Account.displayName участвует в поиске аккаунта: их семантику не менять. Поле находится в существующем диалоге Connection settings, изменения сохраняются явно. Custom name не меняет WebDAV URL/учётные данные.

Finder получает имя существующего mapped domain с прежним ID; reset/remove/recreate ради косметики запрещены. Native API умеет только повторный addDomain по ID, поэтому нужны проверки mapping, timeout и поздних callback; собственные create/remove/rename сериализованы NativeDomainMutationQueue с fresh query внутри fence. Внешнее конкурентное удаление и поздний неизвестный native результат отдельно проверить до выпуска. Сохранённую helper registration0.6.3 пока не удалять в рамках этой функции.

## Прежние решения

Имя приложения Nextcloud Fork сохранить; hostname-only проверен в0.6.4. Swift/native rewrite без Qt отложен владельцем. Подробные альтернативы SFL, системные naming flags и helper-domain hypothesis остаются в MACOS_FINDER_LOCATION_NAMING.md. Данные, сохранённые после test identity incident01.11, не удалять/не сливать без отдельной проверки; real-data reconciliation не завершена.


## 03.11.2026 — штатный quit в preinstall (Unreleased0.6.6)

По просьбе владельца заменяем требование ручного закрытия на single-instance CLI --quit в console user session. AppleScript уже вернул -128 на этом Mac; CLI официально поддержан Application::parseOptions. Не используем SIGKILL, root запуск GUI или остановку File Provider. Ожидание30s ограничено; при невыходе безопасный отказ до замены payload. Текущий опубликованный локально PKG0.6.5 неизменяем; новая упаковка только следующим выпуском.


## 03.11.2026 — редактор имени Finder0.6.7

Реальный screenshot выявил отличие macOS QFormLayout от ранее просмотренного offscreen fixture: поле не растягивалось, Save уходила на край отдельной строки. Заменена структура: label сверху, поле Expanding +Save справа в одном QHBoxLayout, help/reset под полем. Русские fork-only подписи выбираются по фактическому Application ui_lang; upstream translations/ по правилам не редактируется. Для прочих языков сохранён английский source tr fallback. Это ограниченный локальный fallback, не полноценный каталог локализации. Свойство ui_lang учитывает явный override пользователя; системную локаль напрямую не используем.
