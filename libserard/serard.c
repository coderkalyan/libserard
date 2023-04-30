/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2022 OpenCyphal.
/// Author: Pavel Kirienko <pavel@opencyphal.org>
/// Author: Kalyan Sriram <coder.kalyan@gmail.com>

#include "serard.h"
#include "_serard_cavl.h"

/// By default, this macro resolves to the standard assert(). The user can redefine this if necessary.
/// To disable assertion checks completely, make it expand into `(void)(0)`.
#ifndef SERARD_ASSERT
// Intentional violation of MISRA: inclusion not at the top of the file to eliminate unnecessary dependency on assert.h.
#    include <assert.h>  // NOSONAR
// Intentional violation of MISRA: assertion macro cannot be replaced with a function definition.
#    define SERARD_ASSERT(x) assert(x)  // NOSONAR
#endif

#define RX_SESSIONS_PER_SUBSCRIPTION (SERARD_NODE_ID_MAX + 1U)

SERARD_PR
Serard serardInit(const SerardMemoryAllocate memory_allocate, const SerardMemoryFree memory_free)
{
    SERARD_ASSERT(memory_allocate != NULL);
    SERARD_ASSERT(memory_free != NULL);
    Serard serard = {
        .user_reference     = NULL,
        .node_id            = SERARD_NODE_ID_UNSET,
        .memory_allocate    = memory_allocate,
        .memory_free        = memory_free,
        .rx_subscriptions   = {NULL},
    };

    return serard;
}

int8_t serardRxSubscribe(Serard* const               ins,
                         const SerardTransferKind    transfer_kind,
                         const SerardPortID          port_id,
                         const size_t                extent,
                         const SerardMicrosecond     transfer_id_timeout_usec,
                         SerardRxSubscription* const out_subscription)
{
    int8_t ret = -SERARD_ERROR_INVALID_ARGUMENT;
    const size_t tk = (size_t) transfer_kind;

    if ((ins != NULL) && (out_subscription != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS)) {
        ret = canardRxUnsubscribe(ins, transfer_kind, port_id);
        if (ret >= 0) {
            out_subscription->port_id                   = port_id;
            out_subscription->extent                    = extent;
            out_subscription->transfer_id_timeout_usec  = transfer_id_timeout_usec;
            for (size_t i = 0; i < RX_SESSIONS_PER_SUBSCRIPTION; i++) {
                out_subscription->sessions[i] = NULL;
            }

            const SerardTreeNode* const node = cavlSearch(&ins->rx_subscriptions[tk],
                                                          out_subscription,
                                                          &rxSubscriptionPredicateOnStruct,
                                                          &avlTrivialFactory);
            (void) res;
            SERARD_ASSERT(res == &out_subscription->base);
            ret = (ret > 0) ? 0 : 1;
        }
    }

    return ret;
}
