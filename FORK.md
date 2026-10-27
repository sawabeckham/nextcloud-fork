# Nextcloud Fork

Клиент Nextcloud для Mac, в котором **диск в Finder** («Места», File Provider) и **обычная синхронизация
выбранных папок** работают одновременно. В официальном клиенте это два взаимоисключающих режима:
при включении File Provider подключения папок удаляются.

Основа — `nextcloud/desktop` v35.0.0 (ветка `upstream`, тег `v35.0.0`). Изменения форка — в ветке `fork`.

## Что изменено

- Сборочный флаг `NEXTCLOUD_EXPERIMENTAL_HYBRID_SYNC` (`src/common/hybridsyncpolicy.h`): при включении File Provider
  подключения папок остаются, окно «выберите один режим» не появляется, панель папок в настройках аккаунта видна.
  Без флага поведение как у официального клиента.
- Папку для обычной синхронизации нельзя выбрать внутри `~/Library` (там хранилище File Provider —
  `~/Library/CloudStorage`) и нельзя выбрать саму домашнюю папку или её родителей
  (`src/common/hybridsyncpaths.h`, `src/gui/macOS/hybridsyncpaths.h`). Такие уже настроенные папки пропускаются при запуске.
- Флаг `NEXTCLOUD_FORK`: своё имя «Nextcloud Fork», bundle ID `site.manro.nextcloudfork`, свои настройки и запись
  в Связке ключей, схема ссылок `nextcloudfork://`, без автообновления официального клиента.
  Официальный Nextcloud.app и его аккаунты не затрагиваются.
- Сборка пакета: библиотеки клиента копируются в `Contents/Frameworks` до `macdeployqt`.

Ограничение: по словам разработчиков Nextcloud, пока работает расширение File Provider, macOS не запускает
расширение FinderSync. Тогда у обычных папок не будет значков состояния и пунктов в контекстном меню Finder,
но синхронизация будет идти. На деле это ещё не проверено.

Одни и те же файлы сервера можно открыть и через диск, и через синхронизируемую папку. Если править
файл в обоих местах одновременно, получится конфликт (Nextcloud сохранит обе версии).

## Сборка

```
NO_INSTALL=1 ./build.sh          # тесты + клиент + расширения → ~/Library/Caches/RomanenkoSources/nextcloud-fork/package
SKIP_EXTENSIONS=1 ./build.sh     # только клиент, быстрее
```

Зависимости — из Homebrew (Qt 6.12, cmake, KArchive, QtKeychain, libp11, librsvg и др., список — `BREW_DEPS`
в `build.sh`; если чего-то нет, скрипт скажет, что поставить). KDSingleApplication в Homebrew нет — `build.sh`
собирает её сам из релиза v1.2.1 на GitHub.

## Подпись

Клиент и расширение File Provider общаются через группу приложений `<TeamID>.site.manro.nextcloudfork`,
поэтому им нужна подпись сертификатом «Apple Development» с Team ID. Сейчас сертификата нет
(`security find-identity -v -p codesigning` → 0), поэтому сборка неподписанная и `./build.sh` ничего не ставит.
Бесплатный Apple ID в Xcode (Settings → Accounts) даёт сертификат «Apple Development»; хватит ли его для
File Provider, можно проверить только на деле. Дальше: `-DDEVELOPMENT_TEAM=<TeamID>` вместо `NEXTCLOUD_FORK_UNSIGNED`.

## Обновление до новой версии Nextcloud

```
git fetch upstream tag v36.0.0 --depth 1
git rebase --onto v36.0.0 v35.0.0 fork
```

## История

### 0.3 — 2026-10-27
- Перенесено из Codex в Romanenko Sources как git-репозиторий: чистая основа v35.0.0 с GitHub, изменения форка
  отдельными коммитами, без постороннего переформатирования (было ~980 изменённых строк, стало ~180).
- Исправлено: если папку отклоняла проверка путей, клиент обращался к пустому указателю и падал
  (`FolderMan::addFolder`, загрузка старых настроек 3.0). Проверка теперь до того, как трогается журнал в папке.
- `build.sh`: зависимости из Homebrew вместо рабочей папки Codex, сборка в `~/Library/Caches/RomanenkoSources/nextcloud-fork`.
- Исходники вместе со всей папкой Romanenko Sources перенесены из iCloud в `~/Romanenko Sources`.

### 0.2 — 2026-10-23 (Codex)
- Политика совместной работы, проверка путей, брендирование, неподписанная сборка клиента и расширений.
