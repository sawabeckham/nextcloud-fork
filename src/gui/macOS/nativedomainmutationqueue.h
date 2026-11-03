/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QString>
#include <functional>
#include <memory>

namespace OCC::Mac
{

// Completion must be independent of the Qt owner thread: legacy callers can wait there.
class NativeDomainMutationQueue
{
public:
    using Finished = std::function<void()>;
    using Operation = std::function<void(Finished)>;
    using Dispatch = std::function<void(Finished)>;
    using Completion = std::function<void(bool, QString)>;
    using Mutation = std::function<void(Completion)>;
    using QueryCompletion = std::function<void(bool, Mutation, QString)>;
    using Query = std::function<void(QueryCompletion)>;

    explicit NativeDomainMutationQueue(Dispatch dispatch);
    void enqueue(Operation operation);
    // Preserves legacy create/remove semantics: wait for the actual callback, without Qt events.
    void enqueueAndWait(Operation operation);
    // Query runs after earlier mutations finish. An absent domain never becomes a new registration.
    void updateExisting(Query query, Completion completion);

private:
    struct State;
    const std::shared_ptr<State> _state;
};

}
