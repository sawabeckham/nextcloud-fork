<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Подпись единственного подключения Finder: исследование 01.11.2026

## Текущий результат0.6.4

Установленная0.6.4 показывает **только files.romanenko.online** со значком форка при одном настоящем аккаунте. Имя приложения Nextcloud Fork сохранено. Проверены Finder AX/screenshot и штатный перезапуск. Первый уже открытый Finder сохранял старую подпись до открытия нового окна; account/domain reset не потребовался. Подробнее — последний раздел.

## Предыдущий результат0.6.3

Установленная 0.6.3 показывает при одном настоящем подключении **Nextcloud Fork - files.romanenko.online** с одним видимым местом. Проверено в Finder и после штатного перезапуска клиента; подробные runtime evidence приведены в последнем разделе. Далее сохранена история исследования 0.6.2.

## Требование и фактический результат 0.6.2

Владелец требует значок форка и hostname сервера без схемы HTTPS в «Местах», в том числе с одним подключением. Установленная 0.6.2 на основе Nextcloud 35.0.1 **не выполняет это требование**: Finder показывает Nextcloud Fork. Screenshot владельца 15:43:13 подтверждает результат. У оригинального клиента два подключения с именами серверов; этот пример не доказывает такое же поведение с одним.

В домене уже задан files.romanenko.online. Эксперимент 0.6.2 также возвращает это имя из root Item.filename и меняет только metadataVersion корня. `fileproviderctl evaluate` на реальном диске показывает filename/displayName=hostname; исходные remote path и contentVersion не меняются. Шесть новых Swift regression cases проверяют границы, но не доказывают имя в боковой панели. После установки фактическая строка осталась Nextcloud Fork.

## Системное правило на проверенной машине

Система: macOS 27.2, build 26B123. Исследован Apple FileProviderDaemon, UUID 42B432BD-EA59-3F45-99F9-AAE494D48D14. Это исследование конкретной поставленной версии, не гарантия поведения всех macOS.

В `-[FPDProvider shouldHideDomainDisplayName]`, unslid address 0x23AF799BC, дизассемблер показывает сравнение identifier с com.apple.CloudDocs.iCloudDriveFileProviderManaged. Для этого системного провайдера возвращается false. Иначе вызывается relevantDomainsByID, затем count, и результат — count < 2. Имена selector stubs проверены чтением ARM64 ADRP/ADD selector strings в отдельном локальном диагностическом процессе. Эта строка — идентификатор системного провайдера, **не найденный ключ настройки**; не добавлять её в plist/entitlements форка и не выдавать себя за iCloud.

FileProvider.framework содержит внутренние FPProviderDomain.shouldHideDomainDisplayName, domainDisplayName, domainFullDisplayName, providerDisplayName. Это закрытые свойства; public NSFileProviderDomain API такого переключателя не предоставляет в проверенном SDK. Поиск публичных Apple SDK/docs и extension plist keys не обнаружил поддерживаемый override этого правила для стороннего клиента. Отрицательный поиск ограничен перечисленной областью; отсутствие любого возможного SPI не доказано.

REA/Hopper анализ Finder подтвердил: TSidebarItemCell.updateNameAndIcon берёт значение из .name; .name вызывает node helper 0x10005C940. Декомпиляция этого helper сама по себе не установила всю цепочку системного имени. Правило количества установлено отдельно дизассемблированием FileProviderDaemon, а не этим REA trace. Не смешивать обычные диагностические файлы с REA Evidence.

## Доказательства и границы

Все диагностические артефакты вне синхронизируемого репозитория:
`~/Library/Caches/RomanenkoSources/nextcloud-fork/validation-build-20261101/fork-0.6.1/` (каталог переиспользован, версия результата 0.6.2).

- REA Finder artifact SHA256 e7449ccdb76778275fefba432a893f7cfd3257b13ce723d2f3a987a84e7ba85a, Hopper backend, REA 6.7.0.
- rea-finder-evidence-final.json и rea-finder-final.snapshot.json: экспорт и snapshot постоянной временной stdio MCP session; close_binary сообщил verified-cleanup. Настройки Codex/MCP registration не изменялись.
- ev_43ce310d8db70c7c1b431624902050067ed90ba15285a849aa5becf204b678a1: batch декомпиляция sidebar cell методов.
- ev_083dc6533ea566038272b81cff6b69def80a490d0681052d56a8ed586981eef5: node helper.
- ev_e0f68af8ebd334d07322d58f51d142329eb07abdcd561a027ba090898fdd1208: поиск shouldHideDomainDisplayName в строках Finder, пустой результат в инвентаре провайдера; не доказательство отсутствия этого свойства в других образах.
- finder-native-diagnostics/fileproviderdaemon-disassemble.txt: `xcrun dyld_info -disassemble` для системного framework из dyld cache. Адрес getter выше; ARM64 branch после count сравнивает с 2.
- finder-native-diagnostics/nextcloud-provider-metadata-0.1.8.m/.txt: отдельный read-only helper загрузил Apple frameworks в свой процесс, перечислил runtime metadata и разрешил literal/selector strings. Не подключался к живому Finder/fileproviderd и не вызывал setter/removal API. Более ранние 0.1.x оставлены как история диагностики; только 0.1.8 содержит окончательное разрешение selector names. Это временный диагностический код, не часть продукта.

Apple бинарники/процессы, entitlements, provider domains, системный cache и настройки Finder не патчились этим исследованием. Apple decompiled code не переносился в исходники форка. Изменение bundle display name затрагивает имя приложения и не проверено как динамическое решение для произвольных аккаунтов; не выдавать его за готовое исправление.

## Следующий шаг

Установщик, новая палитра и upstream monitor имеют отдельную проверку в CURRENT_STATE; они не закрывают этот пункт. Не выпускать следующий эксперимент как исправление подписи без реальной визуальной проверки одного подключения. Сначала найти поддерживаемый per-domain способ управления представлением либо определить допустимое изменение поведения/названия приложения. Не создавать пустой второй домен и не сбрасывать действующий ради косметики. Следующее runtime изменение требует версии 0.6.3, тестов и новой сборки; установленный подписанный bundle не править на месте.


## Дополнительная проверка после решения сохранить имя приложения

Владелец отклонил переименование самого приложения. Native Finder context menu этой Location не содержит Rename. Старый публичный LSSharedFileList API действительно видит запись форка в FavoriteVolumes (это не Favorites); поэтому направление отдельно проверено, а не отвергнуто по названию API. Root URL совпал с действующим доменом. В отдельном AppKit diagnostic GUI 0.2.3 CUA вызвал update той же записи с inDisplayName=hostname. API возвратил NULL, unified log — NSPOSIXErrorDomain Code=1 Restricted by sandbox, sharedfilelist INSERT_FAILED_ERROR. Имя Nextcloud Fork, ID 2360421295 и count=8 сохранились. Ни одного domain API эта диагностика не вызывала. Это подтверждённый отказ проверенному клиенту API, не доказательство невозможности любых иных системных механизмов. Расширения private permissions/ослабления sandbox не предпринимались. Лог sidebar-list-probe-0.2.3-system.log и diagnostic sources находятся в каталоге артефактов выше.


## Уточнение отказа и прямой update существующего item (16:18–16:25)

Текущий фокус владельца — **только подпись одного подключения**, имя приложения сохранить. Прочие задачи и переписывание отложены.

Проверка diagnostic GUI 0.2.5: sandbox_check(self, NULL, 0)=0, то есть сам процесс не в App Sandbox. Поэтому текст прежнего отказа не доказывал, что sandbox приложения запрещает менять весь список. REA исследован /System/Library/CoreServices/sharedfilelistd, SHA256 c7b69e58e3e9322e064980fb526fb290c1e99617fc78307b1250c71e3953ec90, arm64/Hopper. canAccessItem:list:error проверяет file-read-data для URL из нового bookmark и создаёт именно текст Restricted by sandbox; insert вызывает эту проверку. update существующего item при совпадающем bookmark имеет отдельный путь. Это объясняет отказ прежнего InsertItemURL, не является новым разрешением менять произвольные записи/файлы.

Отдельный diagnostic GUI 0.2.8 использует фактически найденные SFLItem.copyWithZone, setName и SFLGenericList.updateItem:error (внутренний SPI, не публичный стабильный API). Типы методов проверены read-only runtime metadata. Для единственной записи с точным собственным root URL копируется существующий item и меняется только name; новая URL/bookmark не создаётся. CUA нажал Set server label: update возвратил success. GUI и независимый read-only CLI подтвердили имя files.romanenko.online, прежний ID 2360421295 и count=8. Файлы, app name и domain registration код диагностики не меняет. Побайтовый hash bookmark отдельно не измерялся — не приписывать этому тесту такую проверку.

**Видимая строка Finder всё равно осталась Nextcloud Fork:** в текущем окне, в новом CmdN окне и после реального TERM/relaunch Finder. CUA перечитал фактическую строку после каждого шага. Таким образом имя SFL item действительно можно поменять, но это не определяет подпись этой File Provider Location. После проверки нажата Restore original label; независимый readback sidebar-existing-item-restored-readback.log подтвердил Nextcloud Fork/правильный root URL/count=8. Diagnostic GUI и Hopper закрыты, main app не перезапускался.

Дополнительное чтение FileProvider.framework из dyld cache через dyld_info: FPProviderDomain.domainDisplayName (0x196178ECC) возвращает nil при shouldHideDomainDisplayName или shouldHideExtensionName. domainFullDisplayName (0x196178F48) использует providerDisplayName и результат domainDisplayName; скрытое доменное имя в составную подпись не попадает. Это согласуется с ранее установленным правилом count < 2, а не создаёт найденный override. Локальное изменение SFL metadata не решает задачу, его нельзя переносить в продукт как исправление.

REA Evidence: ev_9258c920dd8bf0620b6ebafe18fbca6b50bcc4fa832f2debf7c4652d39d163b9 (access methods); ev_5e696c28491dc4eef5be83e129e91964008ab38cabd7d3833ab53baf97984b3c (access code); ev_a28fcb4a4de791a0928c041fd6eb8a9f31463dc24722a0ff1e2ad096ede0fa5a (insert/update/name handling). Полные records в rea-sharedfilelist-evidence-final.json, snapshot rea-sharedfilelist-final.snapshot.json; close_binary verified-cleanup. Исходники диагностики/ответы/системный log в finder-native-diagnostics и sidebar-service-access.log. dyld_info disassembly FileProvider — отдельный диагностический артефакт, не REA Evidence. Нового работающего решения для одного домена при сохранении имени приложения пока нет.


## Прямая проверка оригинальных исходников по запросу владельца

Проверен именно upstream tag v35.0.1, commit ad4a8503bcbb1749ea4f5955a65688cbe32305df, через git show/git grep, а не только изменённое рабочее дерево форка. Установленный официальный /Applications/Nextcloud.app также имеет short version 35.0.1, build 35.0.1.0. Сопоставление с v35.0.0 показало тот же принцип именования.

1. `src/libsync/account.cpp:199–210`, Account::shortcutName: host через _url.host(), нестандартный порт, затем формат host - prettyName(). Схема HTTPS сюда не попадает. Количество аккаунтов функция не проверяет; prettyName выбирает DAV display name либо davUser, также без проверки количества аккаунтов.
2. `src/gui/macOS/fileproviderdomainmanager.mm:398–430`, addFileProviderDomain: строка 412 всегда берёт account->shortcutName(); строка 430 передаёт её в NSFileProviderDomain initWithIdentifier:displayName:. Один такой domain регистрируется для account; уже существующий возвращается без повторного создания. Условия >=2 для имени здесь нет.
3. Там же :344–375, reconcileDomainDisplayNames: для каждого известного domain сравнивает имя с account->shortcutName и при отличии вызывает addDomain с тем же identifier и новым displayName. Это нормализация имени существующего домена, независимо от количества аккаунтов. Вызов при startup — src/gui/macOS/fileprovidersettingscontroller_mac.mm:69.
4. Найденный `src/gui/accountmanager.cpp:786` if (_accounts.size()==1) только emits accountListInitialized; к выбору подписи Finder отношения не имеет. Git grep по Mac integration/FileProvider sources и account naming не нашёл Nextcloud-side ветки single/multiple-account, выбирающей имя этой Location.

Таким образом в проверенной цепочке исходников оригинала нет отдельной реализации «при двух аккаунтах показать сервер». Клиент всегда передаёт per-account domain displayName. Ранее исследованный системный FPDProvider.shouldHideDomainDisplayName скрывает его, если relevantDomainsByID.count < 2; число именно File Provider domains, а не всех аккаунтов клиента. При двух File Provider domains macOS использует составную подпись providerDisplayName + domainDisplayName, наблюдаемую как Nextcloud - host - user. При одном домене — providerDisplayName. Наш fork уже меняет передаваемое доменное имя на hostname через domainDisplayName helper; это не меняет системное условие видимости имени. Новых runtime изменений/тестов в данном read-only исследовании не было.


## Гипотеза владельца: скрытый фантомный домен (16:42)

Владелец предложил рассмотреть скрытое фантомное подключение. Это новая гипотеза, которая меняет прежнее ограничение исследования «не создавать dummy domains» только в части оценки; реальный вспомогательный domain пока не регистрировался.

Проверен не только count getter, но и его фильтр relevantDomainsByID. В FileProviderDaemon 27.2 predicate block 0x23AF797BC получает nsDomain.isHidden. Для видимого домена он возвращает true; для скрытого исключает только identifier == NSFileProviderDomainDefaultIdentifier. Selector stubs nsDomain/isHidden/identifier/fp_filter и строковая константа разрешены read-only helper nextcloud-provider-metadata-0.2.0. Поэтому hidden домен с отдельным non-default ID **учитывается** в этом счётчике. Раннее предположение, что все скрытые домены могли бы исключаться, не подтвердилось.

Дополнительно выполнен собственный isolated in-memory диагностический тест nextcloud-hidden-domain-predicate-0.1.0.m. Он загружает Apple framework в свой процесс, создаёт публичные NSFileProviderDomain объекты без регистрации, подставляет map mock-domain wrappers в отдельный незарегистрированный FPDProvider и вызывает его реальные relevantDomainsByID/shouldHideDomainDisplayName getters. Ни NSFileProviderManager.addDomain/removeDomain, ни сетевые операции не вызываются. Четыре сценария прошли:

| Объекты | relevant count | hide domain name | Результат |
|---|---:|---|---|
| Один видимый | 1 | true | PASS |
| Два видимых | 2 | false | PASS |
| Видимый + скрытый с отдельным ID | 2 | false | PASS |
| Видимый + скрытый со стандартным ID | 1 | true | PASS |

Это подтверждение работы **системного predicate в памяти**, не end-to-end проверка регистрации, Finder presentation, рестарта или жизненного цикла расширения. Прототип не подключён к живому аккаунту. Public hidden property документирована Apple: https://developer.apple.com/documentation/fileprovider/nsfileproviderdomain/ishidden — домен не виден пользователю, файлы остаются на диске.

Если реализовывать: отдельный зарезервированный ID, hidden=YES, пустое содержимое и без server/credential/network workflow; исключение только для этого ID из orphan cleanup, который сейчас удалит accountless domain; явная обработка такого domain в extension init, где сейчас ожидается настоящий account. Нужны детерминированные tests для 0/1/2 настоящих доменов, error/retry, повторных запусков/создания без дублей и удаления только служебного домена. На одном настоящем домене ожидаемый формат становится как multi-domain: Nextcloud Fork - hostname; фактический вид ещё надо подтвердить. Не применять к живой регистрации как уже готовый fix. Исходники продукта/0.6.2 не менялись; следующая реализация требует 0.6.3.


## 01.11.2026, 18:10 — 0.6.3 установлена; результат Finder подтверждён

После ручной авторизации macOS Installer завершился. `pkgutil --pkg-info site.manro.nextcloudfork.installer` вернул **0.6.3**, location `Applications/RomanenkoApp`; установленный `CFBundleVersion` — **35.0.1.63**. `codesign --verify --deep --strict` установленного приложения успешно выполнен вне sandbox до и после перезапуска. Установщик к итоговой проверке уже не запущен.

Клиент запущен с собственным журналом `--logfile - --logdebug --logflush` в физическом cache `validation-build-20261101/fork-0.6.1/installed-0.6.3-runtime.log`. Журнал подтвердил `Auxiliary domain reconciliation completed` и переход аккаунта в `Connected`. Read-only `fileproviderctl dump site.manro.nextcloudfork.FileProviderExt -l` сохранён в `provider-after-0.6.3.dump`: реальный domain UUID **3fa61ac4-09a2-48ae-b070-25ad1c02d98d** совпадает с `provider-before-0.6.3.dump`; служебный ID **site.manro.nextcloudfork.sidebar-label-helper** отмечен `(hidden)`, `needs-auth: no`, индексирование отключено с причиной `Domain hidden`.

Реальный Finder AX и screenshot подтвердили **«Nextcloud Fork - files.romanenko.online»**, ровно одну видимую строку форка, прежний значок fork и открытие настоящего корня с семью папками. Имя приложения осталось Nextcloud Fork. При текущей ширине боковой панели конец строки визуально сокращается многоточием, как и у оригинальных подключений; AX и заголовок окна содержат полное имя. Служебная строка пользователю не показана. Finder не перезапускался ради результата.

Для проверки постоянства клиент штатно закрыт через `--quit` с предварительным сообщением в чате; диагностическая сессия завершилась exit0. Затем запущен обычный клиент без debug log. Повторный `provider-restart-0.6.3.dump` содержит те же два домена без дублей, UUID настоящего неизменен; Finder AX снова показывает одну строку **«Nextcloud Fork - files.romanenko.online»**. Main PID 17186 и два процесса расширения PID17190/17192 запущены из установленного `/Applications/RomanenkoApp/Nextcloud Fork.app`. Оригинальный Nextcloud остаётся закрытым по уточнению владельца.

Готовый установщик: `~/Github_local/pkg/Nextcloud-Fork-0.6.3-arm64.pkg`, SHA256 **9156803d5aecdcc5b2e3f0238d52f97b8847485c849293c852208080d2fadb08**; предыдущая 0.6.2 сохранена рядом. Тесты и сборка не перезапускались после runtime проверки: программный код не менялся. До упаковки прошли Qt115/115, профиль10/10, Swift388XCTest (1skip) +107SwiftTesting при `--no-parallel`, consuming build и два no-change clang-tidy прохода. Отдельный NextcloudDev check остаётся exit75 из-за official team guard; это не успешная проверка.

Требование подписи одного подключения выполнено на этой macOS. Не проверены reboot/другие macOS/добавление второго настоящего аккаунта через живой UI; соответствующие переходы покрыты автоматическими тестами. Это не подтверждение полной hybrid синхронизации или сверки NAS и сохранённых после прежнего инцидента локальных файлов. Сохранённые локальные файлы и recovery copy не удалялись. Следующий этап только по приоритету владельца: отдельная безопасная проверка полного hybrid сценария на тестовых данных; native rewrite по-прежнему отложен. До любых новых изменений программы повышать версию до 0.6.4, не перезаписывать опубликованный 0.6.3 PKG.


## Уточнение требования владельца после установки 0.6.3

Владелец по screenshot 18:08:35 уточнил: нужна **только иконка + `files.romanenko.online`**, без `Nextcloud Fork -`. Имя самого приложения Nextcloud Fork сохранить. Поэтому 0.6.3 подтверждает появление hostname при одном аккаунте, но **полное требование ещё не выполнено**; прежний вывод о завершении относится только к составной подписи и не должен использоваться как итог текущей задачи.

Изучается источник provider prefix и отдельный системный флаг `shouldHideExtensionName`; публичного параметра в уже проверенных NSFileProviderDomain headers нет. Никакие аккаунты, установленный бандл или domains на этом этапе не меняются. При реализации следующая версия — 0.6.4 с тестом, полноценной сборкой/PKG и визуальной проверкой; 0.6.3 не перезаписывать.


## 0.6.4 — hostname-only, реализация и проверка в процессе

Найдена настройка `NSExtension.NSFileProviderHideExtensionName` типа Boolean. В этой macOS `FPDProviderDescriptor.initWithExtensionRecord:` читает NSNumber из NSExtension dictionary и вызывает `setShouldHideExtensionName:`. Адреса в сохранённом `finder-native-diagnostics/fileproviderdaemon-disassemble.txt`: участок 0x23AF398CC–0x23AF398F0, CFString0x26cfbcbc8=NSFileProviderHideExtensionName,0x26cfbca08=NSExtension, selector stub0x24006def0=setShouldHideExtensionName:. Прямой entitlement gate в этом участке не обнаружен. Это **недокументированный plist key**: отсутствие результатов в публичных SDK headers/Apple web docs не позволяет назвать его поддерживаемым публичным API.

Уточнение старой интерпретации: `shouldHideExtensionName=true` не просто обнуляет domainDisplayName. Реальный `FPProviderDomain.providerDisplayName` в `fileprovider-name-disassemble.txt:150118` возвращает в таком случае domain.displayName; domainDisplayName становится nil, и полное имя равно hostname. Изолированный runtime probe `/private/tmp/nextcloud-hostname-probe/probe-0.1.3.m` (+binary,+log) выполнил **8/8 PASS**, exit0: missing/false/true/wrong-type plist, затем полное имя для обоих состояний hide-domain и hide-extension. Синтетический LS record и собственный subclass прерывают constructor на setter; это доказывает чтение ключа/формирование имени, но не регистрацию или Finder. Никакие реальные domains/системные процессы не изменялись. Чужая реализация в исходники не копировалась; использованы загруженные Apple frameworks локального Mac.

Рабочая версия повышена до0.6.4. `shell_integration/MacOSX/CMakeLists.txt` передаёт только при NEXTCLOUD_FORK buildsetting `OC_FILE_PROVIDER_HIDE_EXTENSION_NAME=1`; Debug/Release FileProviderExt включают plist preprocessing с default0. Условный блок `FileProviderExt/Info.plist` добавляет настоящий Boolean true непосредственно в NSExtension. Official профиль не получает ключ; имя приложения/Bundle IDs/entitlements прежние. Hidden helper из0.6.3 пока оставлен: его удаление не нужно для проверки prefix и было бы отдельным изменением lifecycle.

Regression test `test/hybrid/test_file_provider_location_profile.py`, CTest `ForkFileProviderLocationProfile`, вычисляет реальные CMake commands ON/OFF и preprocess plist через clang для Debug/Release; проверяет тип, положение ключа, отсутствие в official и прежний displayName расширения. Fixture без нового ключа воспроизвёл отказ fork-case; текущий код проходит. Полный профиль **11/11 PASS** (2.48s) в начале build.sh. Полная подпись/PKG/Finder runtime0.6.4 ещё в процессе; установленная пока0.6.3. C++/Swift исходники в этом исправлении не менялись; clang-format/tidy и полный Swift suite не требуются для plist-only delta.


## 01.11.2026, 18:22 — 0.6.4 установлена, hostname-only подтверждён

Полная fork `build.sh` завершилась exit0 (журнал `build/fork-0.6.1/build-0.6.4-full.log`). Native extensions собраны; staging, проверка зависимостей, подпись и PKG прошли. До установки `pkgutil --expand-full` извлёк payload; **codesign --verify --deep --strict source и payload PASS**, main/extension Info.plist, executable и CodeResources совпали. Main build35.0.1.64, расширение build64, `NSExtension.NSFileProviderHideExtensionName` — Boolean true. JSON evidence — `package-0.6.4-validation.json` в физическом build cache.

Installer действительно установил0.6.4 в `/Applications/RomanenkoApp`; macOS приняла уже выданную авторизацию, повторный пароль не потребовался. UI показал успех, Installer закрыт. Receipt0.6.4, installed CFBundleDisplayName=Nextcloud Fork, CFBundleVersion35.0.1.64, installed deep strict codesign PASS. Форк к началу установки уже был закрыт; работающую программу принудительно не завершали.

Запуск installed0.6.4 с собственным application debug log подтвердил Connected и завершение auxiliary reconciliation. Сначала существующее окно Finder ещё показывало старую составную подпись. **Новое окно Finder (CmdN) сразу показало ровно одну строку «files.romanenko.online» со значком fork, без Nextcloud Fork/дефиса/HTTPS.** Выбор этой строки открыл прежний настоящий root `~/Library/CloudStorage/NextcloudFork-files.romanenko.online` с семью папками; заголовок и path bar также hostname-only. AX и screenshot просмотрены. Finder/fileproviderd не перезапускались, системные кеши не правились.

Для проверки постоянства форк штатно закрыт `--quit` после сообщения в чате; debug session34096 exit0. Запущен обычный клиент без debug log: Finder AX снова показывает hostname-only. `provider-after-0.6.4.dump` и `provider-restart-0.6.4.dump` содержат **тот же настоящий UUID3fa61ac4-09a2-48ae-b070-25ad1c02d98d** и один прежний hidden helper site.manro.nextcloudfork.sidebar-label-helper, без дублей. Root path не изменился. После перезапуска mainPID50871, ext50876/50877 из установленного RomanenkoApp; оригинал и Installer не запущены.

Готовый PKG: **`/Users/sawabeckham/Github_local/pkg/Nextcloud-Fork-0.6.4-arm64.pkg`**,75184508bytes,SHA256 **b68b4cb314a2102813b89095d1a073d422e1fa26341248380243d24ea7054944**. Publisher проверил равенство cache copy, оставил0.6.4 и0.6.3; удалил только старый Nextcloud-Fork-0.6.2-arm64.pkg из общей папки по принятой retention policy. Пакеты других программ не менялись.

Проверки этого plist-only изменения: isolated realApple probe8/8; профиль CTest11/11; regression fixture без ключа FAIL/fix PASS; native `xcodebuild build -scheme desktopclient` BUILD SUCCEEDED/exit0 и отсутствие ключа в фактическом official-default Debug extension; полная fork consuming build/PKG/signatures; installed Finder AX+screenshot+restart. C++/Swift код в0.6.4 не менялся, прежние Qt115/115 и Swift388+107 относятся к0.6.3, повторного full suite не было. `git diff --check` пройден. Git fork/b8a3273, старый большой dirty diff сохранён; PR по API по-прежнему только Dependabot1–4, публикации/коммита не было.

Требование **icon + hostname only при одном подключении и прежнем имени приложения** выполнено на текущей macOS27.2/26B123. Ограничения: ключ Apple недокументирован; reboot/другие macOS и живое добавление второго аккаунта не проверены. Hidden helper пока сохранён для минимальности изменения; потенциальное упрощение0.6.5 отдельно с lifecycle tests, не удалять его автоматически ради чистоты. Полная hybrid синхронизация и сверка сохранённых локальных файлов с NAS по-прежнему отдельная незавершённая проверка; сохранённые файлы/recovery copy не трогались. При следующем изменении программы поднять версию до0.6.5. Rewrite без Qt остаётся отложен.
