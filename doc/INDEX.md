<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Документация macOS-форка

Порядок первого чтения: [AGENTS](../AGENTS.md) → [CURRENT_STATE](CURRENT_STATE.md) → [PROJECT_HANDOFF](PROJECT_HANDOFF.md). Сначала сверить Git и метаданные установленной копии: ранние датированные разделы сохраняют историю, последний раздел описывает продолжение.

- [FORK](../FORK.md): назначение, история релизов, проверенная упаковка и подпись.
- [ARCHITECTURE](ARCHITECTURE.md): компоненты и данные.
- [DEVELOPMENT](DEVELOPMENT.md): среда, разрешённые проверки и процедура выпуска.
- [DEVELOPMENT_LOG](DEVELOPMENT_LOG.md): текущие этапы; прежние подробные отчёты остаются в CURRENT_STATE.
- [CHANGELOG](../CHANGELOG.md): Unreleased форка и сохранённая история upstream.
- [DECISIONS](DECISIONS.md): решения и компромиссы.
- [THIRD_PARTY](THIRD_PARTY.md): источники, авторы и лицензии.
- [DOCUMENTATION_POLICY](DOCUMENTATION_POLICY.md): правила обновления комплекта.
- [Подпись Finder](MACOS_FINDER_LOCATION_NAMING.md): исследование системного поведения и доказательства 0.6.4.

Upstream-инструкции [macOS development](macOS-development.md), [терминология](terminology.md) и [стиль](writing-style.md) сохраняются; инструкции владельца о выпуске имеют приоритет.


## Продолжение11.11.2026

Выборочные macOS fixes PR11030/11024/11145/11253 после PKG0.6.8; исходники Unreleased0.6.9, metadata0.6.8. Читать актуальные первые и последние разделы CURRENT_STATE/PROJECT_HANDOFF и provenance THIRD_PARTY. Установленная программа/готовыйPKG не заменяются этим этапом. ARM64 audit:161Mach-O толькоarm64, код других платформ пока сохранён.
