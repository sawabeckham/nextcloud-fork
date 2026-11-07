/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#import <AppKit/AppKit.h>
#import <FileProvider/FileProvider.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QLatin1StringView>
#include <QList>
#include <QLoggingCategory>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <utility>

#include "../../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/ForkFileProviderDomain.h"
#include "auxiliarydomainreconciler.h"
#include "config.h"
#include "domaindisplaynamereconciler.h"
#include "fileprovider.h"
#include "fileproviderdomainmanager.h"
#include "fileprovidersettingscontroller.h"
#include "fileproviderutils.h"
#include "fileproviderutils_mac.h"
#include "fileproviderxpc_mac_utils.h"
#include "nativedomainmutationqueue.h"

#include "common/forkdomaindisplayname.h"
#include "gui/accountmanager.h"
#include "libsync/account.h"

static_assert(__has_feature(objc_arc), "fileproviderdomainmanager requires ARC.");

namespace OCC
{

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

}

namespace
{

/// The domain name shown in Finder, including the fork's local connection label.
QString domainDisplayName(const OCC::AccountPtr &account)
{
#ifdef NEXTCLOUD_FORK
    const auto host = account->url().host();
    const auto hostEndsInPackageExtension = OCC::Mac::FileProviderDomainManager::hasPackageExtension(host);
    const auto connectionName = account->connectionName();
    return OCC::Utility::forkDomainDisplayName(host,
                                               account->url().port(),
                                               account->shortcutName(),
                                               hostEndsInPackageExtension,
                                               connectionName,
                                               OCC::Mac::FileProviderDomainManager::isConnectionNameSupported(connectionName));
#else
    return account->shortcutName();
#endif
}

} // namespace

namespace OCC
{

namespace Mac
{
using namespace Qt::StringLiterals;

class FileProviderDomainManager::MacImplementation
{
public:
    static NativeDomainMutationQueue &mutationQueue()
    {
        static auto queue = NativeDomainMutationQueue([](NativeDomainMutationQueue::Finished operation) {
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{ operation(); });
        });
        return queue;
    }

    MacImplementation() = default;
    ~MacImplementation() = default;

    // MARK: - Synchronous NSFileProviderDomainManager Wrappers

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager addDomain:]`.
     * @return Whether the system confirmed addition. The fork waits for the actual callback.
     */
    bool addDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Adding domain" << domain.identifier;
#ifdef NEXTCLOUD_FORK
        const auto addSucceeded = std::make_shared<bool>(false);
        mutationQueue().enqueueAndWait([domain, addSucceeded](NativeDomainMutationQueue::Finished finished) {
            [NSFileProviderManager addDomain:domain
                           completionHandler:^(NSError *error) {
                               if (error) {
                                   qCWarning(lcMacFileProviderDomainManager) << "Error adding domain:" << error.code << error.localizedDescription;
                               } else {
                                   *addSucceeded = true;
                               }
                               finished();
                           }];
        });
        return *addSucceeded;
#else
        return FileProviderUtils::waitForFileProviderRequest("adding domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [NSFileProviderManager addDomain:domain completionHandler:^(NSError *error) { reply(nil, error); }];
        });
#endif
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager disconnectWithReason:]`.
     */
    void disconnect(NSFileProviderDomain *domain, const QString &message)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Disconnecting domain" << domain.identifier;
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        NSString *const reason = message.toNSString();
        FileProviderUtils::waitForFileProviderRequest("disconnecting domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager disconnectWithReason:reason
                                  options:NSFileProviderManagerDisconnectionOptionsTemporary
                        completionHandler:^(NSError *error) { reply(nil, error); }];
        });
    }

    /**
     * @brief Synchronous and logging wrapper for `[NSFileProviderManager getDomainsWithCompletionHandler:]`.
     */
    QList<NSFileProviderDomain *> getDomains(bool *success = nullptr)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Getting all existing domains...";
        const auto domains = FileProviderUtils::fileProviderDomains(success);

        QList<NSFileProviderDomain *> domainsList;
        for (NSFileProviderDomain *const domain in domains) {
            qCInfo(lcMacFileProviderDomainManager) << "Found domain:" << domain.identifier;
            domainsList.append(domain);
        }
        return domainsList;
    }

    /**
     * @brief Reconnect a specific file provider domain.
     */
    void reconnect(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect domain" << domain.identifier;
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        const auto reconnected = FileProviderUtils::waitForFileProviderRequest("reconnecting domain", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager reconnectWithCompletionHandler:^(NSError *error) { reply(nil, error); }];
        });
        if (reconnected) {
            signalEnumerator(domain);
        }
    }

    /**
     * @brief Ask the extension of a domain to delete its stored password while it can still run.
     *
     * Removal goes ahead on failure because a leftover item is keyed by this domain only.
     */
    void deletePassword(NSFileProviderDomain *domain)
    {
        const auto domainIdentifier = QString::fromNSString(domain.identifier);

        if (!FileProviderXPCUtils::deletePassword(domain, FileProvider::instance()->service()).value_or(false)) {
            qCWarning(lcMacFileProviderDomainManager) << "Removing domain without confirmed password deletion" << domainIdentifier;
        }
    }

    /**
     * @brief Remove a domain and its local support data after confirmed success.
     * @return The preserved-data path, empty for no data, or nullopt if removal was not confirmed.
     */
    std::optional<QString> removeDomain(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing domain" << domain.identifier;
        deletePassword(domain);

#ifdef NEXTCLOUD_FORK
        const auto preservedDataPath = std::make_shared<std::optional<QString>>();
        mutationQueue().enqueueAndWait([domain, preservedDataPath](NativeDomainMutationQueue::Finished finished) {
            [NSFileProviderManager removeDomain:domain
                                           mode:NSFileProviderDomainRemovalModePreserveDirtyUserData
                              completionHandler:^(NSURL *dataURL, NSError *error) {
                                  if (error) {
                                      qCWarning(lcMacFileProviderDomainManager) << "Error removing domain" << error.code << error.localizedDescription;
                                  } else {
                                      *preservedDataPath = QString::fromNSString(dataURL.path);
                                      if (dataURL) {
                                          qCInfo(lcMacFileProviderDomainManager) << "Domain removed with preserved data at:" << dataURL.path;
                                      }
                                      removeFileProviderDomainData(domain.identifier);
                                  }
                                  finished();
                              }];
        });
        return *preservedDataPath;
#else
        id result = nil;
        const auto removed = FileProviderUtils::waitForFileProviderRequest(
            "removing domain",
            ^(FileProviderUtils::RequestReplyHandler reply) {
                [NSFileProviderManager removeDomain:domain
                                               mode:NSFileProviderDomainRemovalModePreserveDirtyUserData
                                  completionHandler:^(NSURL *dataURL, NSError *error) {
                                      if (!reply(dataURL, error) && !error && dataURL) {
                                          qCWarning(lcMacFileProviderDomainManager) << "Late domain removal preserved data at:" << dataURL.path;
                                      }
                                  }];
            },
            &result);
        if (!removed) {
            return std::nullopt;
        }

        NSURL *const preservedDataURL = result;
        if (preservedDataURL) {
            qCInfo(lcMacFileProviderDomainManager) << "Domain removed with preserved data at:" << preservedDataURL.path;
        }
        removeFileProviderDomainData(domain.identifier);
        qCInfo(lcMacFileProviderDomainManager) << "Removed domain" << domain.identifier;
        return QString::fromNSString(preservedDataURL.path);
#endif
    }

    void signalEnumerator(NSFileProviderDomain *domain)
    {
        qCInfo(lcMacFileProviderDomainManager) << "Signaling enumerator for domain" << domain.identifier;
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return;
        }

        FileProviderUtils::waitForFileProviderRequest("signalling enumerator", ^(FileProviderUtils::RequestReplyHandler reply) {
            [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier
                                              completionHandler:^(NSError *error) { reply(nil, error); }];
        });
    }

    /**
     * @brief Synchronous wrapper to get the user-visible URL for a domain's root container.
     */
    QString getUserVisibleUrlForDomain(NSFileProviderDomain *domain)
    {
        if (!domain) {
            return {};
        }
        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (!manager) {
            return {};
        }

        const auto url = FileProviderUtils::userVisibleURLForItem(manager, NSFileProviderRootContainerItemIdentifier);
        return QString::fromNSString(url.path);
    }

    // MARK: - Higher Level Domain Management

    /**
     * @brief Reconnect all existing file provider domains.
     */
    void reconnectAll()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Attempt to reconnect all domains...";
        const auto domains = getDomains();

        for (NSFileProviderDomain *const domain : domains) {
            if (FileProviderDomainManager::isAuxiliaryDomainIdentifier(QString::fromNSString(domain.identifier))) {
                continue;
            }
            reconnect(domain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished reconnecting all domains.";
    }

    /**
     * @brief Update every registered domain's display name to match its account's `shortcutName()`.
     *
     * Walks every registered `NSFileProviderDomain`, looks up its owning account, and if the
     * domain's `displayName` differs from `account->shortcutName()` re-registers the domain
     * in place by calling `addDomain:` again with the same identifier. macOS replaces the
     * registration, keeping the on-disk data intact.
     */
    void reconcileDomainDisplayNames()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Reconciling file provider domain display names...";

        const auto domains = getDomains();
        const auto accountManager = AccountManager::instance();

        for (NSFileProviderDomain *const domain : domains) {
            const auto domainId = QString::fromNSString(domain.identifier);
            if (FileProviderDomainManager::isAuxiliaryDomainIdentifier(domainId)) {
                continue;
            }
            const auto accountState = accountManager->accountFromFileProviderDomainIdentifier(domainId);

            if (!accountState || !accountState->account()) {
                // Orphan domain — not owned by any current account. `removeOrphanedDomains()`
                // in the settings controller is responsible for cleanup; nothing to do here.
                continue;
            }

            const auto currentDisplayName = QString::fromNSString(domain.displayName);
            const auto desiredDisplayName = domainDisplayName(accountState->account());

            if (currentDisplayName == desiredDisplayName) {
                continue;
            }

            qCInfo(lcMacFileProviderDomainManager) << "Updating display name for domain" << domainId << "from" << currentDisplayName << "to"
                                                   << desiredDisplayName;

            NSFileProviderDomain *const updatedDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domain.identifier
                                                                                             displayName:desiredDisplayName.toNSString()];
            updatedDomain.supportsSyncingTrash = YES;
            addDomain(updatedDomain);
        }

        qCInfo(lcMacFileProviderDomainManager) << "Finished reconciling file provider domain display names.";
    }

    /**
     * @brief Remove all file provider domains one by one and also their associated data.
     */
    bool removeAllDomains()
    {
        qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all domains...";
        auto success = false;
        const auto domains = getDomains(&success);
        if (!success) {
            return false;
        }
        for (NSFileProviderDomain *const domain : domains) {
            if (FileProviderDomainManager::isAuxiliaryDomainIdentifier(QString::fromNSString(domain.identifier))) {
                continue;
            }
            if (!removeDomain(domain).has_value()) {
                success = false;
            }
        }
        return success;
    }

    // MARK: - Legacy

    QString addFileProviderDomain(const AccountState *const accountState)
    {
        Q_ASSERT(accountState);
        const auto account = accountState->account();
        Q_ASSERT(account);

        const auto accountId = account->userIdAtHostWithPort();
        const auto existingDomainId = account->fileProviderDomainIdentifier();
        // Use `<server>[:port] - <user>` ordering so the trailing token is the user. This avoids
        // LaunchServices treating the file provider root folder as a bundle when the server's TLD
        // matches a recognised package extension like ".app" (see issues #7979 / #9684).
        // Domains registered with a stale display name (e.g. the legacy U+2024-escaped form, or
        // a name from before `prettyName()` was updated) are normalised on launch by
        // `reconcileDomainDisplayNames()`.
        const auto displayName = domainDisplayName(account);

        const auto pendingDomainId = _pendingAddIdentifiers.value(accountId);
        auto discoverySucceeded = false;
        const auto domains = getDomains(&discoverySucceeded);
        if (!discoverySucceeded) {
            return {};
        }
        for (const auto &identifier : {existingDomainId, pendingDomainId}) {
            if (identifier.isEmpty()) {
                continue;
            }
            for (NSFileProviderDomain *const domain : domains) {
                if (identifier == QString::fromNSString(domain.identifier)) {
                    AccountManager::instance()->setFileProviderDomainIdentifier(accountId, identifier);
                    _pendingAddIdentifiers.remove(accountId);
                    return identifier;
                }
            }
        }

        auto domainId = pendingDomainId;
        if (domainId.isEmpty()) {
            domainId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        // Keep retries on the same identifier; the official profile can return before a late add completes.
        _pendingAddIdentifiers.insert(accountId, domainId);
        NSFileProviderDomain *const domain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString() displayName:displayName.toNSString()];
        domain.supportsSyncingTrash = YES;

        if (!addDomain(domain)) {
            qCWarning(lcMacFileProviderDomainManager) << "Failed to add file provider domain for account" << accountId << "- not persisting domain identifier.";
            return {};
        }

        AccountManager::instance()->setFileProviderDomainIdentifier(accountId, domainId);
        _pendingAddIdentifiers.remove(accountId);

        return domainId;
    }

    static void removeFileProviderDomainData(NSString *const domainIdentifier)
    {
        const auto qDomainIdentifier = QString::fromNSString(domainIdentifier);

        // Remove logs.

        auto logDirectory = OCC::Mac::FileProviderUtils::fileProviderDomainLogDirectory(qDomainIdentifier);

        if (logDirectory.exists()) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing log directory at" << logDirectory.path();
            logDirectory.removeRecursively();
        } else {
            qCInfo(lcMacFileProviderDomainManager) << "Due to lack of existence, not removing log directory at" << logDirectory.path();
        }

        // Remove support data.

        auto supportDirectory = OCC::Mac::FileProviderUtils::fileProviderDomainSupportDirectory(qDomainIdentifier);

        if (supportDirectory.exists()) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing support directory at" << supportDirectory.path();
            supportDirectory.removeRecursively();
        } else {
            qCInfo(lcMacFileProviderDomainManager) << "Due to lack of existence, not removing support directory at" << supportDirectory.path();
        }
    }

private:
    QHash<QString, QString> _pendingAddIdentifiers;
};

// MARK: -

FileProviderDomainManager::FileProviderDomainManager(QObject *const parent)
    : QObject(parent)
{
    qCDebug(lcMacFileProviderDomainManager) << "Initializing...";

    d.reset(new FileProviderDomainManager::MacImplementation());

#ifdef NEXTCLOUD_FORK
    // Unit-test bundles and command-line tools must never manage the client's registrations.
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@APPLICATION_REV_DOMAIN]) {
        return;
    }

    const auto queryNames = [](DomainDisplayNameReconciler::QueryCompletion completion) {
        [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *domains, NSError *error) {
            if (error) {
                completion(false, {}, QString::fromNSString(error.localizedDescription));
                return;
            }
            auto snapshot = DomainDisplayNameReconciler::Domains{};
            for (NSFileProviderDomain *domain in domains) {
                const auto identifier = QString::fromNSString(domain.identifier);
                if (FileProviderDomainManager::isAuxiliaryDomainIdentifier(identifier)) {
                    continue;
                }
                snapshot.insert(identifier,
                                {QString::fromNSString(domain.displayName), [identifier](const QString &name, DomainDisplayNameReconciler::Completion done) {
                                     // The queue fence includes a fresh lookup: an earlier removal cannot
                                     // be undone by re-adding a domain from this outer, potentially stale snapshot.
                                     MacImplementation::mutationQueue().updateExisting(
                                         [identifier, name](NativeDomainMutationQueue::QueryCompletion queried) {
                                             [NSFileProviderManager
                                                 getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *currentDomains, NSError *queryError) {
                                                     if (queryError) {
                                                         queried(false, {}, QString::fromNSString(queryError.localizedDescription));
                                                         return;
                                                     }
                                                     for (NSFileProviderDomain *current in currentDomains) {
                                                         if (QString::fromNSString(current.identifier) != identifier) {
                                                             continue;
                                                         }
                                                         if (@available(macOS 15.0, *)) {
                                                             if (current.volumeUUID) {
                                                                 queried(false, {}, u"External-volume domain names cannot be updated safely."_s);
                                                                 return;
                                                             }
                                                         }
                                                         queried(true,
                                                                 [current, name](NativeDomainMutationQueue::Completion changed) {
                                                                     // displayName is readonly; preserve the latest writable metadata.
                                                                     NSFileProviderDomain *updated =
                                                                         [[NSFileProviderDomain alloc] initWithIdentifier:current.identifier
                                                                                                              displayName:name.toNSString()];
                                                                     updated.hidden = current.hidden;
                                                                     updated.testingModes = current.testingModes;
                                                                     updated.supportsSyncingTrash = current.supportsSyncingTrash;
                                                                     if (@available(macOS 15.0, *)) {
                                                                         updated.userInfo = current.userInfo;
                                                                         updated.supportedKnownFolders = current.supportedKnownFolders;
                                                                     }
                                                                     if (@available(macOS 26.0, *)) {
                                                                         updated.supportsStringSearchRequest = current.supportsStringSearchRequest;
                                                                     }
                                                                     [NSFileProviderManager addDomain:updated
                                                                                    completionHandler:^(NSError *updateError) {
                                                                                        changed(updateError == nil,
                                                                                                QString::fromNSString(updateError.localizedDescription));
                                                                                    }];
                                                                 },
                                                                 {});
                                                         return;
                                                     }
                                                     queried(true, {}, {});
                                                 }];
                                         },
                                         std::move(done));
                                 }});
            }
            completion(true, std::move(snapshot), {});
        }];
    };
    const auto nameTargets = [] {
        auto targets = DomainDisplayNameReconciler::Targets{};
        auto ambiguous = QSet<QString>{};
        for (const auto &state : AccountManager::instance()->accounts()) {
            const auto account = state->account();
            if (!account) {
                continue;
            }
            const auto identifier = account->fileProviderDomainIdentifier();
            if (identifier.isEmpty() || FileProviderDomainManager::isAuxiliaryDomainIdentifier(identifier) || ambiguous.contains(identifier)) {
                continue;
            }
            if (targets.contains(identifier)) {
                targets.remove(identifier);
                ambiguous.insert(identifier);
                continue;
            }
            targets.insert(identifier, {domainDisplayName(account), account.data()});
        }
        return targets;
    };
    _domainDisplayNameReconciler =
        std::make_unique<DomainDisplayNameReconciler>(QString::fromLatin1(NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER), queryNames, nameTargets, this);
    connect(_domainDisplayNameReconciler.get(), &DomainDisplayNameReconciler::finished, this, [](bool success, const QString &error) {
        if (!success) {
            qCWarning(lcMacFileProviderDomainManager) << "Domain name reconciliation failed:" << error;
        }
    });
    connect(_domainDisplayNameReconciler.get(), &DomainDisplayNameReconciler::failed, this, [this](const QString &identifier, const QString &) {
        const auto state = AccountManager::instance()->accountFromFileProviderDomainIdentifier(identifier);
        if (state && state->account()) {
            Q_EMIT domainDisplayNameUpdateFailed(
                state->account()->userIdAtHostWithPort(),
                tr("Finder could not confirm the new connection name. The saved name will be checked again when the application starts."));
        }
    });
    const auto observeConnectionName = [this](AccountState *state) {
        if (const auto account = state->account()) {
            connect(account.data(), &Account::connectionNameChanged, this, &FileProviderDomainManager::reconcileDomainDisplayNames);
        }
    };
    for (const auto &state : AccountManager::instance()->accounts()) {
        observeConnectionName(state.data());
    }
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, observeConnectionName);

    const auto query = [](AuxiliaryDomainReconciler::QueryCompletion completion) {
        [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *domains, NSError *error) {
            if (error) {
                completion(false, {}, QString::fromNSString(error.localizedDescription));
                return;
            }
            auto snapshot = QHash<QString, bool>{};
            for (NSFileProviderDomain *domain in domains) {
                snapshot.insert(QString::fromNSString(domain.identifier), domain.hidden);
            }
            completion(true, snapshot, {});
        }];
    };
    const auto mappedIdentifiers = [] {
        auto identifiers = QStringList{};
        for (const auto &state : AccountManager::instance()->accounts()) {
            if (const auto account = state->account(); account && !account->fileProviderDomainIdentifier().isEmpty()) {
                identifiers.append(account->fileProviderDomainIdentifier());
            }
        }
        return identifiers;
    };
    const auto ensureHidden = [](AuxiliaryDomainReconciler::Completion completion) {
        NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:@NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER
                                                                            displayName:@"Nextcloud Fork Internal"];
        domain.hidden = YES;
        domain.supportsSyncingTrash = NO;
        [NSFileProviderManager addDomain:domain
                       completionHandler:^(NSError *error) { completion(error == nil, QString::fromNSString(error.localizedDescription)); }];
    };
    const auto removeAuxiliary = [](AuxiliaryDomainReconciler::Completion completion) {
        // This path has no account/keychain cleanup and can address only the reserved identifier.
        NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:@NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER
                                                                            displayName:@"Nextcloud Fork Internal"];
        [NSFileProviderManager removeDomain:domain
                                       mode:NSFileProviderDomainRemovalModePreserveDirtyUserData
                          completionHandler:^(NSURL *preservedDataURL, NSError *error) {
                              if (preservedDataURL) {
                                  qCWarning(lcMacFileProviderDomainManager) << "Auxiliary domain data preserved at" << preservedDataURL.path;
                              }
                              completion(error == nil, QString::fromNSString(error.localizedDescription));
                          }];
    };
    _auxiliaryDomainReconciler = std::make_unique<AuxiliaryDomainReconciler>(QString::fromLatin1(NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER),
                                                                             query,
                                                                             mappedIdentifiers,
                                                                             ensureHidden,
                                                                             removeAuxiliary,
                                                                             this);
    connect(_auxiliaryDomainReconciler.get(), &AuxiliaryDomainReconciler::finished, this, [](bool success, const QString &error) {
        if (!success) {
            qCWarning(lcMacFileProviderDomainManager) << "Auxiliary domain reconciliation failed:" << error;
        } else {
            qCInfo(lcMacFileProviderDomainManager) << "Auxiliary domain reconciliation completed.";
        }
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        _domainDisplayNameReconciler.reset();
        _auxiliaryDomainReconciler.reset();
    });
#endif
}

FileProviderDomainManager::~FileProviderDomainManager() = default;

NSFileProviderDomain *FileProviderDomainManager::domainForAccount(const Account *account) const
{
    if (!d || !account) {
        return nil;
    }

    const auto identifier = account->fileProviderDomainIdentifier();

    if (isAuxiliaryDomainIdentifier(identifier)) {
        qCWarning(lcMacFileProviderDomainManager) << "Reserved auxiliary identifier found in account mapping; ignoring it.";
        return nil;
    }

    if (identifier.isEmpty()) {
        return nil;
    }

    const auto domains = d->getDomains();

    for (NSFileProviderDomain *const domain : domains) {
        if (identifier == QString::fromNSString(domain.identifier)) {
            return domain;
        }
    }

    qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found for account" << account->userIdAtHostWithPort() << "with expected identifier"
                                              << identifier;
    return nil;
}

void FileProviderDomainManager::start()
{
    qCDebug(lcMacFileProviderDomainManager) << "Starting...";

    ConfigFile cfg;

    // If an account is deleted from the client, accountSyncConnectionRemoved will be
    // emitted first. So we treat accountRemoved as only being relevant to client
    // shutdowns.
    connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved, this, &FileProviderDomainManager::removeDomainByAccount);

    connect(AccountManager::instance(), &AccountManager::accountRemoved, this, [this](const AccountState *const accountState) {
        const auto trReason = tr("%1 application has been closed. Reopen to reconnect.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
    });

    qCDebug(lcMacFileProviderDomainManager) << "Completed start.";
}

QList<NSFileProviderDomain *> FileProviderDomainManager::getDomains(bool *success) const
{
    if (!d) {
        if (success) {
            *success = false;
        }
        return {};
    }

    return d->getDomains(success);
}

bool FileProviderDomainManager::removeAllDomains()
{
    if (!d) {
        return false;
    }
    const auto removed = d->removeAllDomains();
    requestAuxiliaryDomainReconciliation();
    return removed;
}

void FileProviderDomainManager::reconnectAll()
{
    if (!d) {
        return;
    }

    d->reconnectAll();
}

void FileProviderDomainManager::reconcileDomainDisplayNames()
{
    if (QCoreApplication::closingDown()) {
        return;
    }
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &FileProviderDomainManager::reconcileDomainDisplayNames, Qt::QueuedConnection);
        return;
    }
#ifdef NEXTCLOUD_FORK
    if (_domainDisplayNameReconciler) {
        _domainDisplayNameReconciler->requestReconciliation();
    }
#else
    if (!d) {
        return;
    }

    d->reconcileDomainDisplayNames();
#endif
    requestAuxiliaryDomainReconciliation();
}

bool FileProviderDomainManager::isConnectionNameSupported(const QString &name)
{
    const auto trimmed = name.trimmed();
#ifdef NEXTCLOUD_FORK
    if (!Account::isValidConnectionName(trimmed)) {
        return false;
    }
#endif
    return !hasPackageExtension(trimmed);
}

bool FileProviderDomainManager::hasPackageExtension(const QString &name)
{
    const auto suffixSeparator = name.lastIndexOf(u'.');
    if (suffixSeparator < 0 || suffixSeparator == name.size() - 1) {
        return false;
    }
    const auto suffix = name.mid(suffixSeparator + 1);
    // A suffix can describe both a flat file and a package; the preferred file type
    // alone misses .app and the flat/packaged variants of iWork documents.
    for (UTType *type in [UTType typesWithTag:suffix.toNSString() tagClass:UTTagClassFilenameExtension conformingToType:UTTypeDirectory]) {
        if ([type conformsToType:UTTypePackage]) {
            return true;
        }
    }
    return false;
}

bool FileProviderDomainManager::isAuxiliaryDomainIdentifier(const QString &identifier)
{
#ifdef NEXTCLOUD_FORK
    return identifier == QLatin1StringView(NEXTCLOUD_FORK_AUXILIARY_DOMAIN_IDENTIFIER);
#else
    Q_UNUSED(identifier)
    return false;
#endif
}

void FileProviderDomainManager::requestAuxiliaryDomainReconciliation()
{
    if (QCoreApplication::closingDown()) {
        return;
    }
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &FileProviderDomainManager::requestAuxiliaryDomainReconciliation, Qt::QueuedConnection);
        return;
    }
    if (_auxiliaryDomainReconciler) {
        _auxiliaryDomainReconciler->requestReconciliation();
    }
}

std::optional<QString> FileProviderDomainManager::removeDomain(NSFileProviderDomain *domain)
{
    if (!d || !domain || isAuxiliaryDomainIdentifier(QString::fromNSString(domain.identifier))) {
        return {};
    }

    return d->removeDomain(domain);
}

QString FileProviderDomainManager::addDomainForAccount(const AccountState *const accountState)
{
    if (!d) {
        return {};
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    const auto identifier = d->addFileProviderDomain(accountState);

    if (identifier.isEmpty()) {
        return {};
    }

    // Disconnect the domain when something changes regarding authentication
    connect(accountState, &AccountState::stateChanged, this, [this, accountState] {
        slotAccountStateChanged(accountState);
    });

    reconcileDomainDisplayNames();
    return identifier;
}

void FileProviderDomainManager::signalEnumeratorChanged(const Account *const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);
    NSFileProviderDomain *const domain = domainForAccount(account);

    if (!domain) {
        return;
    }

    d->signalEnumerator(domain);
}

QString FileProviderDomainManager::userVisibleUrlForDomainIdentifier(const QString &domainIdentifier) const
{
    if (!d || domainIdentifier.isEmpty()) {
        return {};
    }

    const auto domains = d->getDomains();

    for (NSFileProviderDomain *const domain : domains) {
        if (domainIdentifier == QString::fromNSString(domain.identifier)) {
            return d->getUserVisibleUrlForDomain(domain);
        }
    }

    qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found with identifier" << domainIdentifier;
    return {};
}

void FileProviderDomainManager::openFileViewerForDomainIdentifier(const QString &domainIdentifier) const
{
    const auto url = userVisibleUrlForDomainIdentifier(domainIdentifier);

    if (url.isEmpty()) {
        return;
    }

    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ [NSURL fileURLWithPath:url.toNSString()] ]];
}

void FileProviderDomainManager::clearInsufficientQuotaErrorAndEnumerate(const QString &domainIdentifier) const
{
    if (!d || domainIdentifier.isEmpty()) {
        return;
    }

    NSFileProviderDomain *targetDomain = nil;

    for (NSFileProviderDomain *const domain : d->getDomains()) {
        if (domainIdentifier == QString::fromNSString(domain.identifier)) {
            targetDomain = domain;
            break;
        }
    }

    if (!targetDomain) {
        qCWarning(lcMacFileProviderDomainManager) << "No file provider domain found with identifier" << domainIdentifier
                                                  << "— cannot clear insufficient-quota error.";
        return;
    }

    NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:targetDomain];

    if (!manager) {
        qCWarning(lcMacFileProviderDomainManager) << "No NSFileProviderManager for domain" << domainIdentifier << "— cannot clear insufficient-quota error.";
        return;
    }

    NSError *const insufficientQuotaError = [NSError errorWithDomain:NSFileProviderErrorDomain code:NSFileProviderErrorInsufficientQuota userInfo:nil];

    qCInfo(lcMacFileProviderDomainManager) << "Clearing insufficient-quota error and signalling working-set enumerator for domain" << domainIdentifier;

    // Per Apple's documentation for `signalErrorResolved(_:completionHandler:)`, calling it
    // tells the system that the previously-returned error no longer applies; the system may
    // then retry the failed operation. Signalling the working set enumerator afterwards
    // nudges the system to actually re-evaluate pending non-uploaded items, mirroring the
    // pattern already used for `.notAuthenticated` in `FileProviderExtension.swift`.
    [manager signalErrorResolved:insufficientQuotaError
               completionHandler:^(NSError *const resolveError) {
                   if (resolveError) {
                       qCWarning(lcMacFileProviderDomainManager) << "Failed to clear insufficient-quota error for domain" << domainIdentifier << ":"
                                                                 << QString::fromNSString(resolveError.localizedDescription);
                   }

                   [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier
                                                     completionHandler:^(NSError *const enumerateError) {
                                                         if (enumerateError) {
                                                             qCWarning(lcMacFileProviderDomainManager)
                                                                 << "Failed to signal working-set enumerator for domain" << domainIdentifier << ":"
                                                                 << QString::fromNSString(enumerateError.localizedDescription);
                                                         }
                                                     }];
               }];
}

void FileProviderDomainManager::slotHandleFileIdsChanged(const OCC::Account *const account, const QList<qint64> &fileIds)
{
    if (!d || !account) {
        return;
    }

    qCInfo(lcMacFileProviderDomainManager) << "Received file ID changes for account" << account->displayName();

    NSFileProviderDomain *const domain = domainForAccount(account);

    if (!domain) {
        qCWarning(lcMacFileProviderDomainManager) << "No domain found for account" << account->displayName();
        return;
    }

    const auto fileProvider = FileProvider::instance();
    const auto xpc = fileProvider ? fileProvider->xpc() : nullptr;
    const auto domainIdentifier = QString::fromNSString(domain.identifier);

    if (xpc && xpc->processFileIdsChanged(domainIdentifier, fileIds)) {
        return;
    }

    qCWarning(lcMacFileProviderDomainManager) << "Could not forward file ID changes to domain" << domainIdentifier << "refreshing without filtering.";
    d->signalEnumerator(domain);
}

std::optional<QString> FileProviderDomainManager::removeDomainByAccount(const AccountState *const accountState)
{
    if (!d) {
        return {};
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();

    if (!account) {
        return {};
    }

    if (account->fileProviderDomainIdentifier().isEmpty()) {
        return QString{};
    }
    if (isAuxiliaryDomainIdentifier(account->fileProviderDomainIdentifier())) {
        qCWarning(lcMacFileProviderDomainManager) << "Reserved auxiliary identifier found in account mapping; refusing removal.";
        return std::nullopt;
    }

    auto success = false;
    const auto domains = d->getDomains(&success);
    if (!success) {
        return std::nullopt;
    }
    for (NSFileProviderDomain *const domain : domains) {
        if (account->fileProviderDomainIdentifier() == QString::fromNSString(domain.identifier)) {
            const auto preservedData = d->removeDomain(domain);
            requestAuxiliaryDomainReconciliation();
            return preservedData;
        }
    }
    return QString{};
}

void FileProviderDomainManager::disconnectFileProviderDomainForAccount(const AccountState *const accountState, const QString &reason)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);
    NSFileProviderDomain *const domain = domainForAccount(account.data());

    if (!domain) {
        return;
    }

    d->disconnect(domain, reason);
}

void FileProviderDomainManager::reconnectFileProviderDomainForAccount(const AccountState *const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);
    NSFileProviderDomain *const domain = domainForAccount(account.data());

    if (!domain) {
        return;
    }

    d->reconnect(domain);
}

void FileProviderDomainManager::slotAccountStateChanged(const AccountState *const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto state = accountState->state();

    qCDebug(lcMacFileProviderDomainManager) << "Account state changed for account:" << accountState->account()->displayName()
                                            << "changing connection status of file provider domain.";

    switch (state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    case AccountState::RedirectDetected:
    case AccountState::NeedToSignTermsOfService: {
        // Disconnect File Provider domain while unauthenticated
        const auto trReason = tr("This account is not authenticated. Please check your account state in the %1 application.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
        break;
    }
    case AccountState::Connected:
        // Provide credentials
        reconnectFileProviderDomainForAccount(accountState);
        break;
    }
}

} // namespace Mac

} // namespace OCC
