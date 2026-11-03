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

# # Прежние решения

Имя приложения Nextcloud Fork сохранить; hostname-only проверен в0.6.4. Swift/native rewrite без Qt отложен владельцем. Подробные альтернативы SFL, системные naming flags и helper-domain hypothesis остаются в MACOS_FINDER_LOCATION_NAMING.md. Данные, сохранённые после test identity incident01.11, не удалять/не сливать без отдельной проверки; real-data reconciliation не завершена.
