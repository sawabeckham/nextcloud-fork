// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
import UniformTypeIdentifiers

/// The accountless helper's only item. Its root never accepts user content.
final class AuxiliaryRootItem: NSObject, NSFileProviderItem {
    private let logger: FileProviderLogger

    init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "AuxiliaryRootItem", log: log)
        super.init()
    }

    var itemIdentifier: NSFileProviderItemIdentifier { .rootContainer }
    var parentItemIdentifier: NSFileProviderItemIdentifier { .rootContainer }
    var filename: String { "Nextcloud Fork" }
    var contentType: UTType { .folder }
    var capabilities: NSFileProviderItemCapabilities { [.allowsReading] }
}
