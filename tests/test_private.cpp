// This software is distributed under the terms of the MIT License.
// Copyright (c) 2022 OpenCyphal

#include <catch.hpp>
#include <cstdio>
#include "exposed.hpp"
#include "serard.h"
#include <iostream>

TEST_CASE("txMakeSessionSpecifier")
{
    REQUIRE(0x1afe == exposed::txMakeSessionSpecifier(SerardTransferKindMessage, 0x1afe));
    REQUIRE(0xdafe == exposed::txMakeSessionSpecifier(SerardTransferKindRequest, 0x1afe));
    REQUIRE(0x9afe == exposed::txMakeSessionSpecifier(SerardTransferKindResponse, 0x1afe));
}

TEST_CASE("txMakeHeader")
{
    struct Serard serard = {
        .user_reference     = nullptr,
        .node_id            = 1234,
        .memory_payload     = {},
        .memory_rx_session  = {},
        .rx_subscriptions   = {nullptr},
    };

    struct SerardTransferMetadata metadata = {
        .priority = SerardPriorityNominal,
        .transfer_kind = SerardTransferKindMessage,
        .port_id = 1234,
        .remote_node_id = 4321,
        .transfer_id = 0,
    };

    std::array<std::uint8_t, 24> buffer = {0};
    std::array<std::uint8_t, 24> expected = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
    exposed::txMakeHeader(&serard, &metadata, &buffer);
    for (std::size_t i = 0; i < 24; i++) {
        REQUIRE(expected[i] == buffer[i]);
    }
}

void* serardAlloc(void* const user_reference, const size_t size)
{
    (void) user_reference;
    return malloc(size);
}

void serardFree(void* const user_reference, const size_t size, void* const pointer)
{
    (void) user_reference;
    (void) size;
    free(pointer);
}

using buffer_t = std::vector<std::uint8_t>;

bool serardEmitter(void* const user_reference, uint8_t data_size, const uint8_t* data)
{
    REQUIRE(data_size > 0);
    REQUIRE(data != NULL);

    auto *const buffer = reinterpret_cast<buffer_t*>(user_reference);
    buffer->insert(buffer->end(), data, data + data_size);

    return true;
}

TEST_CASE("serardTxPush")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .deallocate = &serardFree,
        .allocate = &serardAlloc,
    };
    struct Serard serard = serardInit(allocator, allocator);
    serard.node_id = 4321;

    struct SerardTransferMetadata metadata = {
        .priority = SerardPriorityNominal,
        .transfer_kind = SerardTransferKindMessage,
        .port_id = 1234,
        .remote_node_id = SERARD_NODE_ID_UNSET,
        .transfer_id = 0,
    };

    buffer_t result_buffer;
    auto *const user_reference = reinterpret_cast<void*>(&result_buffer);
    serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);
    
    // printf("push: ");
    // for (unsigned char & it : result_buffer) {
    //     printf("%02x ", it);
    // }
    // printf("\n");
}

TEST_CASE("rxTryParseHeader")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .deallocate = &serardFree,
        .allocate = &serardAlloc,
    };
    struct Serard serard = serardInit(allocator, allocator);
    serard.node_id = 4321;

    std::array<std::uint8_t, 24> buffer = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
    struct SerardTransferMetadata metadata{};

    struct exposed::RxTransferModel out{};
    const bool valid = exposed::rxTryParseHeader(0, buffer.begin(), &out);
    REQUIRE(valid);
    REQUIRE(out.transfer_id == 0);
    REQUIRE(out.transfer_kind == SerardTransferKindMessage);
    REQUIRE(out.destination_node_id == 4321);
    REQUIRE(out.source_node_id == 1234);
    REQUIRE(out.port_id == 1234);
    REQUIRE(out.priority == SerardPriorityNominal);
    REQUIRE(out.timestamp_usec == 0);
}

TEST_CASE("serardRxAccept")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .deallocate = &serardFree,
        .allocate = &serardAlloc,
    };
    struct Serard serard = serardInit(allocator, allocator);
    serard.node_id = 4321;

    struct SerardTransferMetadata metadata = {
        .priority = SerardPriorityNominal,
        .transfer_kind = SerardTransferKindMessage,
        .port_id = 1234,
        .remote_node_id = SERARD_NODE_ID_UNSET,
        .transfer_id = 0,
    };

    buffer_t result_buffer;
    auto *const user_reference = reinterpret_cast<void*>(&result_buffer);
    serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);
    for (unsigned char & it : result_buffer) {
        printf("%02x ", it);
    }
    printf("\n");

    struct SerardRxSubscription sub{};
    serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 0, 1000, &sub);
    struct SerardReassembler reassembler{};
    size_t payload_size = result_buffer.size();
    struct SerardRxTransfer out{};
    struct SerardRxSubscription* out_sub = nullptr;
    const int8_t ret = serardRxAccept(&serard,
                                      &reassembler,
                                      0,
                                      &payload_size,
                                      result_buffer.data(),
                                      &out,
                                      &out_sub);
    printf("%d\n", ret);
    REQUIRE(ret == 1);
    REQUIRE(out_sub == &sub);
}
