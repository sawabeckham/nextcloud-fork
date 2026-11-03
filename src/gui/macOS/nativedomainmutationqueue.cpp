/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "nativedomainmutationqueue.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <utility>

namespace OCC::Mac
{
using namespace Qt::StringLiterals;

struct NativeDomainMutationQueue::State : std::enable_shared_from_this<State> {
    explicit State(Dispatch dispatcher)
        : dispatch(std::move(dispatcher))
    {
    }

    void start(Operation operation)
    {
        const auto state = shared_from_this();
        dispatch([state, operation = std::move(operation)] {
            const auto finished = std::make_shared<std::atomic_bool>(false);
            operation([state, finished] {
                if (finished->exchange(true)) {
                    return;
                }
                auto next = Operation{};
                {
                    const auto lock = std::lock_guard(state->mutex);
                    if (state->pending.empty()) {
                        state->active = false;
                        return;
                    }
                    next = std::move(state->pending.front());
                    state->pending.pop_front();
                }
                state->start(std::move(next));
            });
        });
    }

    const Dispatch dispatch;
    std::mutex mutex;
    std::deque<Operation> pending;
    bool active = false;
};

NativeDomainMutationQueue::NativeDomainMutationQueue(Dispatch dispatch)
    : _state(std::make_shared<State>(std::move(dispatch)))
{
}

void NativeDomainMutationQueue::enqueue(Operation operation)
{
    {
        const auto lock = std::lock_guard(_state->mutex);
        if (_state->active) {
            _state->pending.push_back(std::move(operation));
            return;
        }
        _state->active = true;
    }
    _state->start(std::move(operation));
}

void NativeDomainMutationQueue::updateExisting(Query query, Completion completion)
{
    enqueue([query = std::move(query), completion = std::move(completion)](Finished finished) {
        const auto queried = std::make_shared<std::atomic_bool>(false);
        query([finished, completion, queried](bool success, Mutation mutation, QString error) {
            if (queried->exchange(true)) {
                return;
            }
            if (!success || !mutation) {
                finished();
                completion(false, error.isEmpty() ? u"The registered File Provider domain is no longer available."_s : error);
                return;
            }
            const auto mutated = std::make_shared<std::atomic_bool>(false);
            mutation([finished, completion, mutated](bool mutationSucceeded, QString mutationError) {
                if (mutated->exchange(true)) {
                    return;
                }
                finished();
                completion(mutationSucceeded, std::move(mutationError));
            });
        });
    });
}

}
