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
- Значки — оригинальные Nextcloud с надписью «fork»: значок приложения (Dock, Finder;
  `theme/colored/NextcloudFork-macOS-icon.svg`), монохромные значки строки меню, как у официального клиента,
  с «fork» под символом (`theme/*/nextcloudfork/`), и символ диска в «Местах»
  (`FileProviderExt/Assets.xcassets/FileProviderForkSymbol.symbolset`, задаётся `OC_FILE_PROVIDER_SYMBOL`).
  Их рисует `swift admin/fork/make-fork-icons.swift` из оригинальных файлов; значки строки меню
  квадратные — Qt вписывает значок в квадрат. Finder кэширует значки «Мест»: после смены — `killall Finder`.
- Домен File Provider называется только адресом сервера (`files.example.org`, с портом, если он
  нестандартный) — так называется папка диска в `~/Library/CloudStorage` (`src/common/forkdomaindisplayname.h`).
  Если адрес оканчивается на расширение пакета вроде `.app`, остаётся имя «сервер - пользователь»: иначе Finder
  показал бы папку диска как программу. В боковой панели Finder при одном диске пишет имя приложения
  («Nextcloud Fork»), а не имя домена: это поведение macOS, API, чтобы его поменять, нет.

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

`build.sh` подписывает пакет сертификатом «Apple Development» Personal Team Apple ID (`TEAM_ID`, по умолчанию
`EW76LLFDSH`): порядок и флаги как в `admin/osx/mac-crafter` (изнутри наружу, hardened runtime, entitlements из сборки).
Профиль подготовки не нужен: в entitlements нет ключей `com.apple.developer.*`, только песочница, сеть и группа
приложений `EW76LLFDSH.site.manro.nextcloudfork`. Сертификат создаётся в Xcode → Settings → Apple Accounts →
Personal Team → Manage Certificates → + → Apple Development. `UNSIGNED=1 ./build.sh` — без подписи.

Подпись разработчика не нотаризована: на этом Mac приложение запускается, на другом Gatekeeper его не пустит.

## Обновление до новой версии Nextcloud

```
git fetch upstream tag v36.0.0 --depth 1
git rebase --onto v36.0.0 v35.0.0 fork
```

## История

### 0.5 — 2026-10-28
- Значки с надписью «fork»: приложение, строка меню (монохромные по умолчанию), символ диска в «Местах».
- Домен диска — только адрес сервера; уже созданный домен переименовывается при запуске.

### 0.4 — 2026-10-28
- Подпись Personal Team `EW76LLFDSH`: все 107 исполняемых файлов пакета, у клиента и расширений одна группа приложений.
- Пакет ещё не запускался: работа File Provider с бесплатной командой и вместе с синхронизацией папок не проверена.

### 0.3 — 2026-10-27
- Перенесено из Codex в Romanenko Sources как git-репозиторий: чистая основа v35.0.0 с GitHub, изменения форка
  отдельными коммитами, без постороннего переформатирования (было ~980 изменённых строк, стало ~180).
- Исправлено: если папку отклоняла проверка путей, клиент обращался к пустому указателю и падал
  (`FolderMan::addFolder`, загрузка старых настроек 3.0). Проверка теперь до того, как трогается журнал в папке.
- `build.sh`: зависимости из Homebrew вместо рабочей папки Codex, сборка в `~/Library/Caches/RomanenkoSources/nextcloud-fork`.
- Исходники вместе со всей папкой Romanenko Sources перенесены из iCloud в `~/Romanenko Sources`.

### 0.2 — 2026-10-23 (Codex)
- Политика совместной работы, проверка путей, брендирование, неподписанная сборка клиента и расширений.
