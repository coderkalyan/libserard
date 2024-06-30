// This software is distributed under the terms of the MIT License.
// Copyright (c) 2022 OpenCyphal

#include <catch.hpp>
#include <cstdio>
#include "exposed.hpp"
#include "serard.h"
#include <random>
#include <limits>

// Instead of actually encoding a payload with cobs, this test case
// tests the behavior of cobsEncodeByte to different states and inputs
// For payload testing, see the cobsEncodeIncremental test case
TEST_CASE("cobsEncodeByte")
{
    // Testing input bytes != COBS_FRAME_DELIMITER, and ignoring the 255:
    // byte jump limit
    // * these should always be passed through to the output pointer
    // * the encoder state machine should increment its write pointer
    // * the "chunk" pointer should remain stable
    {
        exposed::CobsEncoder encoder(0, 0);
        std::uint8_t         out_byte = exposed::COBS_FRAME_DELIMITER;

        using u8_limits = std::numeric_limits<std::uint8_t>;
        for (std::uint32_t val = u8_limits::min(); val <= u8_limits::max(); val++)
        {
            const std::uint8_t in_byte = static_cast<std::uint8_t>(val);
            if (in_byte == exposed::COBS_FRAME_DELIMITER)
            {
                continue;
            }

            out_byte    = exposed::COBS_FRAME_DELIMITER;
            encoder.loc = 0;  // to make sure we write into out_byte correctly
            exposed::cobsEncodeByte(&encoder, in_byte, &out_byte);
            REQUIRE(out_byte == in_byte);
            REQUIRE(encoder.loc == 1);
            REQUIRE(encoder.chunk == 0x00);
        }
    }

    // Now consider the scenario where the write pointer is at 0x9,
    // the chunk pointer is at 0x0, and a frame delimiter byte is input:
    // * the byte at 0x9 should be set to the delimiter value
    // * the write pointer should increment to 0xA
    // * the chunk pointer should write (0x9 - 0x0 = 0x9) to the 0x0 byte
    // * the chunk pointer should advance to the old write pointer (0x9)
    // * none of the intermediate bytes should be touched
    {
        std::array<std::uint8_t, 10> buffer;
        std::fill(buffer.begin(), buffer.end(), 0xAA);
        exposed::CobsEncoder encoder(0x9, 0x0);
        exposed::cobsEncodeByte(&encoder, exposed::COBS_FRAME_DELIMITER, buffer.data());

        REQUIRE(buffer[0x9] == exposed::COBS_FRAME_DELIMITER);
        REQUIRE(encoder.loc == 0xA);
        REQUIRE(buffer[0x0] == 0x9);
        REQUIRE(encoder.chunk == 0x9);
        for (std::size_t i = 0x1; i <= 0x8; i++)
        {
            REQUIRE(buffer[i] == 0xAA);
        }
    }

    // Next, consider the same test case as above, but for the *maximum*
    // allowed chunk distance, that doesn't require a second chunk pointer.
    // That is, chunk pointer == 0x00, and write pointer == 0x0FE.
    {
        std::array<std::uint8_t, 0xFF> buffer;
        std::fill(buffer.begin(), buffer.end(), 0xAA);
        exposed::CobsEncoder encoder(0xFE, 0x0);
        const std::uint8_t   input_byte = exposed::COBS_FRAME_DELIMITER;
        exposed::cobsEncodeByte(&encoder, input_byte, buffer.data());

        REQUIRE(buffer[0xFE] == exposed::COBS_FRAME_DELIMITER);
        REQUIRE(encoder.loc == 0xFF);
        REQUIRE(((uint16_t) buffer[0x0]) == 0xFE);
        REQUIRE(encoder.chunk == 0xFE);
        for (std::size_t i = 0x1; i <= 0xFD; i++)
        {
            REQUIRE(buffer[i] == 0xAA);
        }
    }

    // Finally, consider the case where the chunk pointer is at 0x00
    // and the write pointer is at 0xFE, but we *don't receive a
    // delimiter byte:
    // * byte 0xFE should be set to the input byte
    // * byte 0x00 should be set to 0xFF to encode that the chunk is of
    //   maximum size and the next write pointer will be found 0xFF bytes
    //   ahead
    // * byte 0xFF, the new chunk pointer, should be zeroed
    // * the chunk pointer should be set to 0xFF
    // * the write pointer should be incremented twice to 0x100
    {
        std::array<std::uint8_t, 0x200> buffer;
        std::fill(buffer.begin(), buffer.end(), 0xAA);
        exposed::CobsEncoder encoder(0xFE, 0x0);
        const std::uint8_t   input_byte = 0xBB;
        exposed::cobsEncodeByte(&encoder, input_byte, buffer.data());

        REQUIRE(buffer[0xFE] == input_byte);
        REQUIRE(buffer[0x00] == 0xFF);
        REQUIRE(((uint16_t) buffer[0xFF]) == 0x00);
        REQUIRE(encoder.chunk == 0xFF);
        REQUIRE(encoder.loc == 0x100);
        for (std::size_t i = 0x1; i <= 0xFD; i++)
        {
            REQUIRE(buffer[i] == 0xAA);
        }
    }
}

TEST_CASE("cobsEncodeIncremental")
{
    {
        const std::array<std::uint8_t, 2> in       = {0x00, 0x00};
        const std::array<std::uint8_t, 3> expected = {0x01, 0x01, 0x00};
        std::array<std::uint8_t, 3>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x01, 0x00};
        const std::array<std::uint8_t, 3> expected = {0x02, 0x01, 0x00};
        std::array<std::uint8_t, 3>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x02, 0x00};
        const std::array<std::uint8_t, 3> expected = {0x02, 0x02, 0x00};
        std::array<std::uint8_t, 3>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        const std::array<std::uint8_t, 2> in       = {0x03, 0x00};
        const std::array<std::uint8_t, 3> expected = {0x02, 0x03, 0x00};
        std::array<std::uint8_t, 3>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        const std::array<std::uint8_t, 3> in       = {0x00, 0x00, 0x00};
        const std::array<std::uint8_t, 4> expected = {0x01, 0x01, 0x01, 0x00};
        std::array<std::uint8_t, 4>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        const std::array<std::uint8_t, 3> in       = {0x00, 0x01, 0x00};
        const std::array<std::uint8_t, 4> expected = {0x01, 0x02, 0x01, 0x00};
        std::array<std::uint8_t, 4>       out{};
        exposed::CobsEncoder              encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }

    {
        std::array<std::uint8_t, 256> in{};
        for (std::size_t i = 0x01; i <= 0xFF; i++)
        {
            in[i - 1] = static_cast<std::uint8_t>(i);
        }
        in[255] = 0x00;

        std::array<std::uint8_t, 258> expected{};
        expected[0] = 0xFF;
        for (std::size_t i = 0x01; i <= 0xFE; i++)
        {
            expected[i] = static_cast<std::uint8_t>(i);
        }
        expected[255] = 0x02;
        expected[256] = 0xFF;
        expected[257] = 0x00;

        std::array<std::uint8_t, 258> out{};
        exposed::CobsEncoder          encoder;
        exposed::cobsEncodeIncremental(&encoder, in.size(), in.data(), out.data());
        REQUIRE(out == expected);
    }
}

TEST_CASE("cobsEncodingSize")
{
    REQUIRE(exposed::cobsEncodingSize(1U) == 2U);
    for (std::size_t i = 2U; i <= 254; i++)
    {
        REQUIRE(exposed::cobsEncodingSize(i) == (i + 1U));
    }
    REQUIRE(exposed::cobsEncodingSize(255U) == (255U + 2U));

    std::random_device                                       dev;
    std::mt19937                                             rng(dev());
    constexpr std::size_t                                    max = std::numeric_limits<std::uint32_t>::max();
    std::uniform_int_distribution<std::mt19937::result_type> dist(1, max);

    for (std::size_t i = 0; i < 1000; i++)
    {
        const auto rand     = dist(rng);
        const auto overhead = static_cast<std::size_t>(std::ceil(static_cast<double>(rand) / 254.0));
        REQUIRE(exposed::cobsEncodingSize(rand) == (rand + overhead));
    }
}

// TODO: cobs decoding
TEST_CASE("HeaderCRC")
{
    exposed::HeaderCRC crc = 0xFFFFU;

    crc = exposed::headerCRCAdd(crc, 1, "1");
    crc = exposed::headerCRCAdd(crc, 1, "2");
    crc = exposed::headerCRCAdd(crc, 1, "3");
    REQUIRE(0x5BCEU == crc);
    crc = exposed::headerCRCAdd(crc, 6, "456789");
    REQUIRE(0x29B1U == crc);
}

static void testTransferCRC(void)
{
    exposed::TransferCRC crc = exposed::TRANSFER_CRC_INITIAL;
    crc                      = exposed::transferCRCAdd(crc, 3, "123");
    crc                      = exposed::transferCRCAdd(crc, 6, "456789");
    REQUIRE(0x1CF96D7CUL == crc);
    REQUIRE(0xE3069283UL == (crc ^ exposed::TRANSFER_CRC_OUTPUT_XOR));
    crc = exposed::transferCRCAdd(crc,
                                  4,
                                  "\x83"  // Least significant byte first.
                                  "\x92"
                                  "\x06"
                                  "\xE3");
    REQUIRE(0xB798B438UL == crc);
    REQUIRE(0x48674BC7UL == (crc ^ exposed::TRANSFER_CRC_OUTPUT_XOR));
}

TEST_CASE("hostToLittle")
{
    {
        std::array<std::uint8_t, 2> buffer{};
        exposed::hostToLittle16(0x0102, buffer.data());
        const std::array<std::uint8_t, 2> expected = {0x02, 0x01};
        REQUIRE(expected == buffer);
    }

    {
        std::array<std::uint8_t, 4> buffer{};
        exposed::hostToLittle32(0x01020304, buffer.data());
        const std::array<std::uint8_t, 4> expected = {0x04, 0x03, 0x02, 0x01};
        REQUIRE(expected == buffer);
    }

    {
        std::array<std::uint8_t, 8> buffer{};
        exposed::hostToLittle64(0x0102030405060708, buffer.data());
        const std::array<std::uint8_t, 8> expected = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
        REQUIRE(expected == buffer);
    }
}

TEST_CASE("littleToHost")
{
    {
        std::array<std::uint8_t, 2> buffer = {0x02, 0x01};
        const auto                  ret    = exposed::littleToHost16(buffer.data());
        REQUIRE(0x0102 == ret);
    }

    {
        std::array<std::uint8_t, 4> buffer = {0x04, 0x03, 0x02, 0x01};
        const auto                  ret    = exposed::littleToHost32(buffer.data());
        REQUIRE(0x01020304 == ret);
    }

    {
        std::array<std::uint8_t, 8> buffer = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
        const auto                  ret    = exposed::littleToHost64(buffer.data());
        REQUIRE(0x0102030405060708 == ret);
    }
}

TEST_CASE("txMakeSessionSpecifier")
{
    REQUIRE(0x1afe == exposed::txMakeSessionSpecifier(SerardTransferKindMessage, 0x1afe));
    REQUIRE(0xdafe == exposed::txMakeSessionSpecifier(SerardTransferKindRequest, 0x1afe));
    REQUIRE(0x9afe == exposed::txMakeSessionSpecifier(SerardTransferKindResponse, 0x1afe));
}

TEST_CASE("txMakeHeader")
{
    struct Serard serard = {
        .user_reference    = nullptr,
        .node_id           = 1234,
        .memory_payload    = {},
        .memory_rx_session = {},
        .rx_subscriptions  = {nullptr},
    };

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityNominal,
            .transfer_kind  = SerardTransferKindMessage,
            .port_id        = 1234,
            .remote_node_id = 4321,  // TODO: is this correct?
            .transfer_id    = 0,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
        exposed::txMakeHeader(&serard, &metadata, &buffer);
        REQUIRE(expected == buffer);
    }

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityImmediate,
            .transfer_kind  = SerardTransferKindResponse,
            .port_id        = 1234,
            .remote_node_id = 4321,
            .transfer_id    = 0,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x01, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x84,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xAC, 0x89};
        exposed::txMakeHeader(&serard, &metadata, &buffer);
        REQUIRE(expected == buffer);
    }

    {
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityOptional,
            .transfer_kind  = SerardTransferKindRequest,
            .port_id        = 5678,
            .remote_node_id = 4321,
            .transfer_id    = 0xCAFEB0BAUL,
        };

        std::array<std::uint8_t, 24> buffer   = {0};
        std::array<std::uint8_t, 24> expected = {0x01, 0x07, 0xD2, 0x04, 0xE1, 0x10, 0x2E, 0xD6,
                                                 0xBA, 0xB0, 0xFE, 0xCA, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x47, 0xE3};
        exposed::txMakeHeader(&serard, &metadata, &buffer);
        REQUIRE(expected == buffer);
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

    auto* const buffer = reinterpret_cast<buffer_t*>(user_reference);
    buffer->insert(buffer->end(), data, data + data_size);

    return true;
}

TEST_CASE("serardTxPush")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .deallocate     = &serardFree,
        .allocate       = &serardAlloc,
    };

    {
        struct Serard serard = serardInit(allocator, allocator);
        serard.node_id       = 4321;

        // TODO: reject illegal port ids
        struct SerardTransferMetadata metadata = {
            .priority       = SerardPrioritySlow,
            .transfer_kind  = SerardTransferKindRequest,
            .port_id        = 511,
            .remote_node_id = 1234,
            .transfer_id    = 0xCAFEB0BAUL,
        };

        buffer_t    result_buffer;
        auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
        serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);

        std::array<std::uint8_t, 31> expected = {0x00, 0x0d, 0x01, 0x06, 0xe1, 0x10, 0xd2, 0x04, 0xff, 0xc1, 0xba,
                                                 0xb0, 0xfe, 0xca, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x80,
                                                 0x01, 0x03, 0x6a, 0xc6, 0x01, 0x01, 0x01, 0x01, 0x00};
        REQUIRE(result_buffer.size() == expected.size());
        for (std::size_t i = 0; i < result_buffer.size(); i++)
        {
            REQUIRE(result_buffer[i] == expected[i]);
        }
    }

    {
        struct Serard serard = serardInit(allocator, allocator);
        serard.node_id       = 1234;

        struct SerardTransferMetadata metadata = {
            .priority       = SerardPriorityNominal,
            .transfer_kind  = SerardTransferKindMessage,
            .port_id        = 1234,
            .remote_node_id = SERARD_NODE_ID_UNSET,
            .transfer_id    = 0,
        };

        buffer_t    result_buffer;
        auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
        // uavcan.primitive.String.1 containing string “012345678”
        std::array<std::uint8_t, 9> payload = {'0', '1', '2', '3', '4', '5', '6', '7', '8'};
        serardTxPush(&serard, &metadata, payload.size(), payload.data(), user_reference, &serardEmitter);

        std::array<std::uint8_t, 40> expected = {0x00, 0x09, 0x01, 0x04, 0xd2, 0x04, 0xff, 0xff, 0xd2, 0x04,
                                                 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                                 0x02, 0x80, 0x01, 0x10, 0x08, 0x12, 0x30, 0x31, 0x32, 0x33,
                                                 0x34, 0x35, 0x36, 0x37, 0x38, 0xd2, 0xee, 0x56, 0xc8, 0x00};
        REQUIRE(result_buffer.size() == expected.size());
        for (std::size_t i = 0; i < result_buffer.size(); i++)
        {
            REQUIRE(result_buffer[i] == expected[i]);
        }
    }
}

TEST_CASE("rxTryParseHeader")
{
    struct SerardMemoryResource allocator = {
        .user_reference = nullptr,
        .deallocate     = &serardFree,
        .allocate       = &serardAlloc,
    };
    struct Serard serard = serardInit(allocator, allocator);
    serard.node_id       = 4321;

    std::array<std::uint8_t, 24> buffer = {0x01, 0x04, 0xD2, 0x04, 0xE1, 0x10, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x4A, 0xD6};
    struct SerardTransferMetadata metadata
    {};

    struct exposed::RxTransferModel out
    {};
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

// TEST_CASE("serardRxAccept")
// {
//     struct SerardMemoryResource allocator = {
//         .user_reference = nullptr,
//         .deallocate     = &serardFree,
//         .allocate       = &serardAlloc,
//     };
//     struct Serard serard = serardInit(allocator, allocator);
//     serard.node_id       = 4321;
//
//     struct SerardTransferMetadata metadata = {
//         .priority       = SerardPriorityNominal,
//         .transfer_kind  = SerardTransferKindMessage,
//         .port_id        = 1234,
//         .remote_node_id = SERARD_NODE_ID_UNSET,
//         .transfer_id    = 0,
//     };
//
//     buffer_t    result_buffer;
//     auto* const user_reference = reinterpret_cast<void*>(&result_buffer);
//     serardTxPush(&serard, &metadata, 0, nullptr, user_reference, &serardEmitter);
//     for (unsigned char& it : result_buffer)
//     {
//         printf("%02x ", it);
//     }
//     printf("\n");
//
//     struct SerardRxSubscription sub
//     {};
//     serardRxSubscribe(&serard, SerardTransferKindMessage, 1234, 0, 1000, &sub);
//     struct SerardReassembler reassembler
//     {};
//     size_t payload_size = result_buffer.size();
//     struct SerardRxTransfer out
//     {};
//     struct SerardRxSubscription* out_sub = nullptr;
//     const int8_t ret = serardRxAccept(&serard, &reassembler, 0, &payload_size, result_buffer.data(), &out, &out_sub);
//     printf("%d\n", ret);
//     REQUIRE(ret == 1);
//     REQUIRE(out_sub == &sub);
// }
