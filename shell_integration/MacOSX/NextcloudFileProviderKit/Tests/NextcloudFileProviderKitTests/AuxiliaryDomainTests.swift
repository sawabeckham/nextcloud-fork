// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

import FileProvider
@testable import NextcloudFileProviderKit
import NextcloudFileProviderXPC
import Testing
import TestInterface

@Suite("Accountless auxiliary domain")
struct AuxiliaryDomainTests {
    private func makeExtension() -> FileProviderExtension {
        FileProviderExtension(
            domain: NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER), displayName: "Helper"),
            bundleIdentifier: NEXTCLOUD_FORK_FILE_PROVIDER_EXTENSION_IDENTIFIER
        )
    }

    @Test func exactIdentityOnly() {
        for bundle in [nil, "com.nextcloud.desktopclient.FileProviderExt", NEXTCLOUD_FORK_FILE_PROVIDER_EXTENSION_IDENTIFIER + ".tests"] {
            #expect(!FileProviderExtension.isAuxiliaryDomain(bundleIdentifier: bundle, domainIdentifier: NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER))
        }
        for identifier in ["", "NSFileProviderDomainDefaultIdentifier", NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER + "-other"] {
            #expect(!FileProviderExtension.isAuxiliaryDomain(bundleIdentifier: NEXTCLOUD_FORK_FILE_PROVIDER_EXTENSION_IDENTIFIER, domainIdentifier: identifier))
        }
        #expect(makeExtension().isAuxiliaryDomain)
    }

    @Test func emptyReadOnlyRootAndUnknownItem() async {
        await confirmation("Immediate callbacks complete exactly once", expectedCount: 3) { completed in
            let subject = makeExtension()
            let progress = subject.item(for: .rootContainer, request: NSFileProviderRequest()) { item, error in
                completed()
                #expect(error == nil)
                #expect(item?.itemIdentifier == .rootContainer)
                #expect(item?.capabilities == [.allowsReading])
            }
            #expect(progress.isFinished)
            progress.cancel()
            let unknown = subject.item(for: NSFileProviderItemIdentifier("unknown"), request: NSFileProviderRequest()) { item, error in
                completed()
                #expect(item == nil)
                #expect((error as? NSFileProviderError)?.code == .noSuchItem)
            }
            #expect(unknown.isFinished)
            let fetch = subject.fetchContents(for: .rootContainer, version: nil, request: NSFileProviderRequest()) { url, item, error in
                #expect(url == nil)
                completed()
                #expect(item == nil)
                #expect((error as? NSFileProviderError)?.code == .noSuchItem)
            }
            #expect(fetch.isFinished)
        }
    }

    @Test func emptyEnumerationAndInvalidation() async throws {
        let subject = makeExtension()
        for identifier in [NSFileProviderItemIdentifier.rootContainer, .workingSet] {
            let enumerator = try subject.enumerator(for: identifier, request: NSFileProviderRequest())
            let observer = MockEnumerationObserver(enumerator: enumerator)
            try await observer.enumerateItems()
            #expect(observer.items.isEmpty)
            #expect(observer.page == nil)
            let changes = MockChangeObserver(enumerator: enumerator)
            try await changes.enumerateChanges()
            #expect(changes.changedItems.isEmpty)
            #expect(changes.deletedItemIdentifiers.isEmpty)
            #expect(changes.finishes.count == 1)
            #expect(changes.finishes.first?.moreComing == false)
            let auxiliaryEnumerator = try #require(enumerator as? AuxiliaryEnumerator)
            await confirmation("Current anchor completes before invalidation") { completed in
                auxiliaryEnumerator.currentSyncAnchor { anchor in
                    completed()
                    #expect(anchor != nil)
                }
            }
            enumerator.invalidate()
            await confirmation("Current anchor completes after invalidation") { completed in
                auxiliaryEnumerator.currentSyncAnchor { anchor in
                    completed()
                    #expect(anchor == nil)
                }
            }
            await #expect(throws: (any Error).self) { try await MockChangeObserver(enumerator: enumerator).enumerateChanges() }
            await #expect(throws: (any Error).self) { try await MockEnumerationObserver(enumerator: enumerator).enumerateItems() }
        }
        #expect(throws: (any Error).self) { try subject.enumerator(for: .trashContainer, request: NSFileProviderRequest()) }
    }

    @Test func accountAndServicesRemainInert() async {
        await confirmation("Account and service callbacks finish once", expectedCount: 5) { completed in
            let subject = makeExtension()
            #expect(subject.manager == nil)
            #expect(subject.blockSyncObservation == nil)
            let progress = subject.supportedServiceSources(for: .rootContainer) { services, error in
                completed()
                #expect(services?.isEmpty == true)
                #expect(error == nil)
            }
            #expect(progress.isFinished)
            progress.cancel()
            #expect(throws: (any Error).self) { try subject.makeListenerEndpoint() }
            #expect(!subject.listener(NSXPCListener.anonymous(), shouldAcceptNewConnection: NSXPCConnection(serviceName: "invalid.test")))
            subject.setupDomainAccount(user: "unused", userId: "unused", serverUrl: "https://invalid.test", password: "unused") { error in
                completed()
                #expect((error as? NSFileProviderError)?.code == .notAuthenticated)
            }
            subject.configureAccount(withUser: "unused", userId: "unused", serverUrl: "https://invalid.test", password: "unused", userAgent: "test")
            subject.setIgnoreList(["unused"])
            subject.removeAccountConfig()
            subject.deletePassword { completed(); #expect($0) }
            subject.hasDirtyUserData { completed(); #expect(!$0) }
            subject.processFileIdsChanged([1]) { completed(); #expect($0) }
            #expect(subject.ncAccount == nil)
            #expect(subject.dbManager == nil)
            #expect(subject.ignoredFiles == nil)
            #expect(subject.connections.isEmpty)
            let unusedAccount = Account(user: "unused", id: "unused", serverUrl: "https://invalid.test", password: "unused")
            #expect(!subject.storeAccount(unusedAccount, generation: 0))
            #expect(!subject.publishAccount(unusedAccount, generation: 0))
            #expect(subject.removeAccount())
            #expect(subject.setupGeneration == 0)
            await #expect(throws: (any Error).self) { try await subject.awaitAccount(timeoutNanoseconds: 0) }
        }
    }

    @Test func directlyConstructedUIServiceCannotAuthenticate() async {
        let service = FPUIExtensionServiceSource(fpExtension: makeExtension())
        #expect(throws: (any Error).self) { try service.makeListenerEndpoint() }
        #expect(!service.listener(NSXPCListener.anonymous(), shouldAcceptNewConnection: NSXPCConnection(serviceName: "invalid.test")))
        let error = await service.authenticate()
        #expect((error as? NSFileProviderError)?.code == .notAuthenticated)
        #expect(await service.userAgent() == nil)
        #expect(await service.credentials().count == 0)
        #expect(await service.itemServerPath(identifier: .rootContainer) == nil)
        #expect(await service.presentUnifiedSharingDialog(identifier: .rootContainer, localPath: "unused") == false)
    }

    @Test func mutationAndOtherNativeCallbacksCompleteImmediately() async {
        await confirmation("Immediate callbacks complete exactly once", expectedCount: 6) { completed in
            let subject = makeExtension()
            let request = NSFileProviderRequest()
            let item = AuxiliaryRootItem(log: subject.log)
            let version = NSFileProviderItemVersion(contentVersion: Data(), metadataVersion: Data())
            let create = subject.createItem(basedOn: item, fields: [], contents: nil, request: request) { item, _, _, error in
                completed()
                #expect(item == nil)
                #expect((error as? CocoaError)?.code == .fileWriteNoPermission)
            }
            let modify = subject.modifyItem(item, baseVersion: version, changedFields: [], contents: nil, request: request) { item, _, _, error in
                completed()
                #expect(item == nil)
                #expect((error as? CocoaError)?.code == .fileWriteNoPermission)
            }
            let delete = subject.deleteItem(identifier: .rootContainer, baseVersion: version, request: request) { error in
                completed()
                #expect((error as? CocoaError)?.code == .fileWriteNoPermission)
            }
            let action = subject.performAction(identifier: .keepDownloaded, onItemsWithIdentifiers: [.rootContainer]) { error in
                completed()
                #expect((error as? CocoaError)?.code == .fileWriteNoPermission)
            }
            let thumbnails = subject.fetchThumbnails(for: [.rootContainer], requestedSize: .zero, perThumbnailCompletionHandler: { _, _, _ in
                Issue.record("No thumbnail work should start")
            }) { error in
                completed()
                #expect((error as? NSFileProviderError)?.code == .noSuchItem)
            }
            for progress in [create, modify, delete, action, thumbnails] {
                #expect(progress.isFinished); progress.cancel()
            }
            subject.materializedItemsDidChange { completed() }
            subject.notifyChange()
            #expect(subject.syncActions.isEmpty)
            #expect(subject.errorActions.isEmpty)
        }
    }
}
