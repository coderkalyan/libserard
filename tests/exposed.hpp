// This software is distributed under the terms of the MIT License.
// Copyright (c) 2024 Cyphal Development Team.

#pragma once

#include "serard.h"
#include <cstdarg>
#include <cstdint>
#include <limits>
#include <stdexcept>

/// Definitions that are not exposed by the library but that are needed for testing.
/// Please keep them in sync with the library by manually updating as necessary.
namespace exposed
{
constexpr std::uint8_t COBS_FRAME_DELIMITER = 0U;

using HeaderCRC   = std::uint16_t;
using TransferCRC = std::uint32_t;

struct CobsEncoder
{
    std::size_t loc;
    std::size_t chunk;

    CobsEncoder()
    {
        loc   = 1U;
        chunk = 0U;
    }

    CobsEncoder(std::size_t const _loc, std::size_t const _chunk)
    {
        loc   = _loc;
        chunk = _chunk;
    }
};

// struct TxItem final : CanardTxQueueItem
// {
//     [[nodiscard]] auto getPayloadByte(const std::size_t offset) const -> std::uint8_t
//     {
//         return reinterpret_cast<const std::uint8_t*>(frame.payload)[offset];
//     }
//
//     [[nodiscard]] auto getTailByte() const
//     {
//         if (frame.payload_size < 1U)
//         {
//             // Can't use REQUIRE because it is not thread-safe.
//             throw std::logic_error("Can't get the tail byte because the frame payload is empty.");
//         }
//         return getPayloadByte(frame.payload_size - 1U);
//     }
//
//     [[nodiscard]] auto isStartOfTransfer() const { return (getTailByte() & 128U) != 0; }
//     [[nodiscard]] auto isEndOfTransfer() const { return (getTailByte() & 64U) != 0; }
//     [[nodiscard]] auto isToggleBitSet() const { return (getTailByte() & 32U) != 0; }
//
//     ~TxItem()                                 = default;
//     TxItem(const TxItem&)                     = delete;
//     TxItem(const TxItem&&)                    = delete;
//     auto operator=(const TxItem&) -> TxItem&  = delete;
//     auto operator=(const TxItem&&) -> TxItem& = delete;
// };

struct RxSession
{
    struct SerardTreeNode base                    = {0};
    SerardMicrosecond     transfer_timestamp_usec = std::numeric_limits<std::uint64_t>::max();
    SerardNodeID          source_node_id          = 0U;
    std::size_t           total_payload_size      = 0U;
    std::size_t           payload_size            = 0U;
    std::uint8_t*         payload                 = nullptr;
    // TransferCRC       calculated_crc          = 0U;
    SerardTransferID transfer_id = std::numeric_limits<std::uint8_t>::max();
    // std::uint8_t      redundant_iface_index   = std::numeric_limits<std::uint8_t>::max();
    // bool              toggle                  = false;
};

struct RxTransferModel
{
    SerardMicrosecond   timestamp_usec      = std::numeric_limits<std::uint64_t>::max();
    SerardPriority      priority            = SerardPriorityOptional;
    SerardTransferKind  transfer_kind       = SerardTransferKindMessage;
    SerardPortID        port_id             = std::numeric_limits<std::uint16_t>::max();
    SerardNodeID        source_node_id      = SERARD_NODE_ID_UNSET;
    SerardNodeID        destination_node_id = SERARD_NODE_ID_UNSET;
    SerardTransferID    transfer_id         = std::numeric_limits<std::uint8_t>::max();
    std::size_t         payload_size        = 0U;
    const std::uint8_t* payload             = nullptr;
};

struct FrameHeaderModel
{
    uint8_t  version                    = 0U;
    uint8_t  priority                   = SerardPriorityOptional;
    uint16_t source_node_id             = SERARD_NODE_ID_UNSET;
    uint16_t destination_node_id        = SERARD_NODE_ID_UNSET;
    uint16_t data_specifier_snm         = 0U;
    uint64_t transfer_id                = std::numeric_limits<std::uint64_t>::max();
    uint32_t frame_index_eot            = 0U;
    uint16_t user_data                  = 0U;
    uint8_t  header_crc16_big_endian[2] = {0};
};

// Extern C effectively discards the outer namespaces.
extern "C" {

void   cobsEncodeByte(struct CobsEncoder* const encoder, std::uint8_t const byte, std::uint8_t* const out_buffer);
void   cobsEncodeIncremental(struct CobsEncoder* const encoder,
                             size_t const              payload_size,
                             const std::uint8_t* const payload,
                             std::uint8_t* const       out_buffer);
size_t cobsEncodingSize(size_t const payload);

constexpr TransferCRC TRANSFER_CRC_INITIAL    = 0xFFFFFFFFUL;
constexpr TransferCRC TRANSFER_CRC_OUTPUT_XOR = 0xFFFFFFFFUL;

void hostToLittle16(uint16_t const in, uint8_t* const out);
void hostToLittle32(uint32_t const in, uint8_t* const out);
void hostToLittle64(uint64_t const in, uint8_t* const out);
auto littleToHost16(const uint8_t* const in) -> std::uint16_t;
auto littleToHost32(const uint8_t* const in) -> std::uint32_t;
auto littleToHost64(const uint8_t* const in) -> std::uint64_t;

auto headerCRCAddByte(const std::uint16_t crc, const std::size_t size, const void* const bytes) -> std::uint16_t;
auto headerCRCAdd(const HeaderCRC crc, const size_t size, const void* const data) -> HeaderCRC;
auto transferCRCAdd(const uint32_t crc, const size_t size, const void* const data) -> TransferCRC;

auto txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind, const SerardPortID port_id) -> std::uint16_t;
auto txMakeHeader(const struct Serard* const                 serard,
                  const struct SerardTransferMetadata* const metadata,
                  void* const                                buffer) -> void;

auto rxTryParseHeader(const SerardMicrosecond       timestamp_usec,
                      const std::uint8_t* const     payload,
                      struct RxTransferModel* const out) -> bool;
// auto rxTryParseFrame(const CanardMicrosecond  timestamp_usec,
//                      const CanardFrame* const frame,
//                      RxFrameModel* const      out_result) -> bool;
//
// auto rxSessionWritePayload(CanardInstance* const ins,
//                            RxSession* const      rxs,
//                            const std::size_t     extent,
//                            const std::size_t     payload_size,
//                            const void* const     payload) -> std::int8_t;
//
// void rxSessionRestart(CanardInstance* const ins, RxSession* const rxs);
//
// auto rxSessionUpdate(CanardInstance* const     ins,
//                      RxSession* const          rxs,
//                      const RxFrameModel* const frame,
//                      const std::uint8_t        redundant_iface_index,
//                      const CanardMicrosecond   transfer_id_timeout_usec,
//                      const std::size_t         extent,
//                      CanardRxTransfer* const   out_transfer) -> std::int8_t;
}
}  // namespace exposed
