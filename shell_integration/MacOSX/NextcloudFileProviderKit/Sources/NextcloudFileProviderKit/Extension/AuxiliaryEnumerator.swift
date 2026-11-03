// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider

/// Only used after the extension and reserved helper identity both match exactly.
final class AuxiliaryEnumerator: NSObject, NSFileProviderEnumerator {
    private let logger: FileProviderLogger

    init(log: any FileProviderLogging) {
        logger = FileProviderLogger(category: "AuxiliaryEnumerator", log: log)
        super.init()
    }

    private static let anchor = NSFileProviderSyncAnchor(Data("auxiliary-empty-v1".utf8))
    private let stateLock = NSLock()
    private var didInvalidate = false

    private var invalidated: Bool {
        stateLock.lock()
        defer { stateLock.unlock() }
        return didInvalidate
    }

    func invalidate() {
        stateLock.lock()
        didInvalidate = true
        stateLock.unlock()
    }

    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt _: NSFileProviderPage) {
        guard !invalidated else {
            observer.finishEnumeratingWithError(CocoaError(.userCancelled))
            return
        }
        observer.finishEnumerating(upTo: nil)
    }

    func enumerateChanges(for observer: NSFileProviderChangeObserver, from _: NSFileProviderSyncAnchor) {
        guard !invalidated else {
            observer.finishEnumeratingWithError(CocoaError(.userCancelled))
            return
        }
        observer.finishEnumeratingChanges(upTo: Self.anchor, moreComing: false)
    }

    func currentSyncAnchor(completionHandler: @escaping (NSFileProviderSyncAnchor?) -> Void) {
        completionHandler(invalidated ? nil : Self.anchor)
    }
}

extension FileProviderExtension {
    static func completedAuxiliaryProgress() -> Progress {
        let progress = Progress(totalUnitCount: 1)
        progress.completedUnitCount = 1
        return progress
    }
}
