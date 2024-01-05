/// This software is distributed under the terms of the MIT License.
/// Copyright (c) 2022-2023 OpenCyphal.
/// Author: Pavel Kirienko <pavel@opencyphal.org>
/// Author: Kalyan Sriram <coder.kalyan@gmail.com>

#include "serard.h"
#include "_serard_cavl.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

// TODO: remove
#include <stdio.h>
#include <stdbool.h>
// --------------------------------------------- BUILD CONFIGURATION ---------------------------------------------

/// By default, this macro resolves to the standard assert(). The user can redefine this if necessary.
/// To disable assertion checks completely, make it expand into `(void)(0)`.
#ifndef SERARD_ASSERT
// Intentional violation of MISRA: inclusion not at the top of the file to eliminate unnecessary dependency on assert.h.
#    include <assert.h>  // NOSONAR
// Intentional violation of MISRA: assertion macro cannot be replaced with a function definition.
#    define SERARD_ASSERT(x) assert(x)  // NOSONAR
#endif

/// This macro is needed for testing and for library development.
/// TODO
// #ifndef SERARD_PRIVATE
// #    define SERARD_PRIVATE static inline
// #endif
#define SERARD_PRIVATE

#ifndef SERARD_UNUSED
#    define SERARD_UNUSED(x) ((void) (x))
#endif

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#    error "Unsupported language: ISO C99 or a newer version is required."
#endif

// --------------------------------------------- COMMON DEFINITIONS ---------------------------------------------

#define BITS_PER_BYTE 8U
#define BYTE_MAX 0xFFU

#define HEADER_CRC_SIZE_BYTES 2U
#define HEADER_SIZE_NO_CRC 22U
#define HEADER_SIZE (HEADER_SIZE_NO_CRC + HEADER_CRC_SIZE_BYTES)
// #define HEADER_VERSION (UINT32_C(1) << 4U)
#define HEADER_VERSION 1U

#define COBS_OVERHEAD_RATE 254U
#define COBS_FRAME_DELIMITER 0U

// #define OFFSET_PRIORITY_BITS 5U
#define SERVICE_NOT_MESSAGE 0x8000U
#define REQUEST_NOT_RESPONSE 0x4000U
#define FRAME_INDEX 0U
#define END_OF_TRANSFER (1U << 31U)

SERARD_PRIVATE struct SerardTreeNode* avlTrivialFactory(void* const user_reference)
{
    return (struct SerardTreeNode*) user_reference;
}

// --------------------------------------------- HEADER CRC ---------------------------------------------

typedef uint16_t HeaderCRC;

#define HEADER_CRC_INITIAL 0xFFFFU
#define HEADER_CRC_RESIDUE 0x0000U

SERARD_PRIVATE HeaderCRC headerCRCAddByte(const HeaderCRC crc, const uint8_t byte)
{
    static const uint16_t CRCTable[256] = {
        0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U, 0x8108U, 0x9129U, 0xA14AU, 0xB16BU,
        0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU, 0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52B5U, 0x4294U, 0x72F7U, 0x62D6U,
        0x9339U, 0x8318U, 0xB37BU, 0xA35AU, 0xD3BDU, 0xC39CU, 0xF3FFU, 0xE3DEU, 0x2462U, 0x3443U, 0x0420U, 0x1401U,
        0x64E6U, 0x74C7U, 0x44A4U, 0x5485U, 0xA56AU, 0xB54BU, 0x8528U, 0x9509U, 0xE5EEU, 0xF5CFU, 0xC5ACU, 0xD58DU,
        0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76D7U, 0x66F6U, 0x5695U, 0x46B4U, 0xB75BU, 0xA77AU, 0x9719U, 0x8738U,
        0xF7DFU, 0xE7FEU, 0xD79DU, 0xC7BCU, 0x48C4U, 0x58E5U, 0x6886U, 0x78A7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
        0xC9CCU, 0xD9EDU, 0xE98EU, 0xF9AFU, 0x8948U, 0x9969U, 0xA90AU, 0xB92BU, 0x5AF5U, 0x4AD4U, 0x7AB7U, 0x6A96U,
        0x1A71U, 0x0A50U, 0x3A33U, 0x2A12U, 0xDBFDU, 0xCBDCU, 0xFBBFU, 0xEB9EU, 0x9B79U, 0x8B58U, 0xBB3BU, 0xAB1AU,
        0x6CA6U, 0x7C87U, 0x4CE4U, 0x5CC5U, 0x2C22U, 0x3C03U, 0x0C60U, 0x1C41U, 0xEDAEU, 0xFD8FU, 0xCDECU, 0xDDCDU,
        0xAD2AU, 0xBD0BU, 0x8D68U, 0x9D49U, 0x7E97U, 0x6EB6U, 0x5ED5U, 0x4EF4U, 0x3E13U, 0x2E32U, 0x1E51U, 0x0E70U,
        0xFF9FU, 0xEFBEU, 0xDFDDU, 0xCFFCU, 0xBF1BU, 0xAF3AU, 0x9F59U, 0x8F78U, 0x9188U, 0x81A9U, 0xB1CAU, 0xA1EBU,
        0xD10CU, 0xC12DU, 0xF14EU, 0xE16FU, 0x1080U, 0x00A1U, 0x30C2U, 0x20E3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
        0x83B9U, 0x9398U, 0xA3FBU, 0xB3DAU, 0xC33DU, 0xD31CU, 0xE37FU, 0xF35EU, 0x02B1U, 0x1290U, 0x22F3U, 0x32D2U,
        0x4235U, 0x5214U, 0x6277U, 0x7256U, 0xB5EAU, 0xA5CBU, 0x95A8U, 0x8589U, 0xF56EU, 0xE54FU, 0xD52CU, 0xC50DU,
        0x34E2U, 0x24C3U, 0x14A0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U, 0xA7DBU, 0xB7FAU, 0x8799U, 0x97B8U,
        0xE75FU, 0xF77EU, 0xC71DU, 0xD73CU, 0x26D3U, 0x36F2U, 0x0691U, 0x16B0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
        0xD94CU, 0xC96DU, 0xF90EU, 0xE92FU, 0x99C8U, 0x89E9U, 0xB98AU, 0xA9ABU, 0x5844U, 0x4865U, 0x7806U, 0x6827U,
        0x18C0U, 0x08E1U, 0x3882U, 0x28A3U, 0xCB7DU, 0xDB5CU, 0xEB3FU, 0xFB1EU, 0x8BF9U, 0x9BD8U, 0xABBBU, 0xBB9AU,
        0x4A75U, 0x5A54U, 0x6A37U, 0x7A16U, 0x0AF1U, 0x1AD0U, 0x2AB3U, 0x3A92U, 0xFD2EU, 0xED0FU, 0xDD6CU, 0xCD4DU,
        0xBDAAU, 0xAD8BU, 0x9DE8U, 0x8DC9U, 0x7C26U, 0x6C07U, 0x5C64U, 0x4C45U, 0x3CA2U, 0x2C83U, 0x1CE0U, 0x0CC1U,
        0xEF1FU, 0xFF3EU, 0xCF5DU, 0xDF7CU, 0xAF9BU, 0xBFBAU, 0x8FD9U, 0x9FF8U, 0x6E17U, 0x7E36U, 0x4E55U, 0x5E74U,
        0x2E93U, 0x3EB2U, 0x0ED1U, 0x1EF0U,
    };
    return (uint16_t) ((uint16_t) (crc << BITS_PER_BYTE) ^
                       CRCTable[(uint16_t) ((uint16_t) (crc >> BITS_PER_BYTE) ^ byte) & BYTE_MAX]);
}

SERARD_PRIVATE HeaderCRC headerCRCAdd(const HeaderCRC crc, const size_t size, const void* const data)
{
    SERARD_ASSERT((data != NULL) || (size == 0U));
    uint16_t      out = crc;
    const uint8_t* p   = (const uint8_t*) data;
    for (size_t i = 0; i < size; i++)
    {
        out = headerCRCAddByte(out, *p);
        ++p;
    }
    return out;
}

// --------------------------------------------- TRANSFER CRC ---------------------------------------------

typedef uint32_t TransferCRC;

#define TRANSFER_CRC_INITIAL 0xFFFFFFFFUL
#define TRANSFER_CRC_OUTPUT_XOR 0xFFFFFFFFUL
#define TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR 0xB798B438UL
#define TRANSFER_CRC_RESIDUE_AFTER_OUTPUT_XOR (TRANSFER_CRC_RESIDUE_BEFORE_OUTPUT_XOR ^ TRANSFER_CRC_OUTPUT_XOR)
#define TRANSFER_CRC_SIZE_BYTES 4U

SERARD_PRIVATE TransferCRC transferCRCAddByte(const TransferCRC crc, const uint8_t byte)
{
    static const TransferCRC CRCTable[256] = {
        0x00000000UL, 0xF26B8303UL, 0xE13B70F7UL, 0x1350F3F4UL, 0xC79A971FUL, 0x35F1141CUL, 0x26A1E7E8UL, 0xD4CA64EBUL,
        0x8AD958CFUL, 0x78B2DBCCUL, 0x6BE22838UL, 0x9989AB3BUL, 0x4D43CFD0UL, 0xBF284CD3UL, 0xAC78BF27UL, 0x5E133C24UL,
        0x105EC76FUL, 0xE235446CUL, 0xF165B798UL, 0x030E349BUL, 0xD7C45070UL, 0x25AFD373UL, 0x36FF2087UL, 0xC494A384UL,
        0x9A879FA0UL, 0x68EC1CA3UL, 0x7BBCEF57UL, 0x89D76C54UL, 0x5D1D08BFUL, 0xAF768BBCUL, 0xBC267848UL, 0x4E4DFB4BUL,
        0x20BD8EDEUL, 0xD2D60DDDUL, 0xC186FE29UL, 0x33ED7D2AUL, 0xE72719C1UL, 0x154C9AC2UL, 0x061C6936UL, 0xF477EA35UL,
        0xAA64D611UL, 0x580F5512UL, 0x4B5FA6E6UL, 0xB93425E5UL, 0x6DFE410EUL, 0x9F95C20DUL, 0x8CC531F9UL, 0x7EAEB2FAUL,
        0x30E349B1UL, 0xC288CAB2UL, 0xD1D83946UL, 0x23B3BA45UL, 0xF779DEAEUL, 0x05125DADUL, 0x1642AE59UL, 0xE4292D5AUL,
        0xBA3A117EUL, 0x4851927DUL, 0x5B016189UL, 0xA96AE28AUL, 0x7DA08661UL, 0x8FCB0562UL, 0x9C9BF696UL, 0x6EF07595UL,
        0x417B1DBCUL, 0xB3109EBFUL, 0xA0406D4BUL, 0x522BEE48UL, 0x86E18AA3UL, 0x748A09A0UL, 0x67DAFA54UL, 0x95B17957UL,
        0xCBA24573UL, 0x39C9C670UL, 0x2A993584UL, 0xD8F2B687UL, 0x0C38D26CUL, 0xFE53516FUL, 0xED03A29BUL, 0x1F682198UL,
        0x5125DAD3UL, 0xA34E59D0UL, 0xB01EAA24UL, 0x42752927UL, 0x96BF4DCCUL, 0x64D4CECFUL, 0x77843D3BUL, 0x85EFBE38UL,
        0xDBFC821CUL, 0x2997011FUL, 0x3AC7F2EBUL, 0xC8AC71E8UL, 0x1C661503UL, 0xEE0D9600UL, 0xFD5D65F4UL, 0x0F36E6F7UL,
        0x61C69362UL, 0x93AD1061UL, 0x80FDE395UL, 0x72966096UL, 0xA65C047DUL, 0x5437877EUL, 0x4767748AUL, 0xB50CF789UL,
        0xEB1FCBADUL, 0x197448AEUL, 0x0A24BB5AUL, 0xF84F3859UL, 0x2C855CB2UL, 0xDEEEDFB1UL, 0xCDBE2C45UL, 0x3FD5AF46UL,
        0x7198540DUL, 0x83F3D70EUL, 0x90A324FAUL, 0x62C8A7F9UL, 0xB602C312UL, 0x44694011UL, 0x5739B3E5UL, 0xA55230E6UL,
        0xFB410CC2UL, 0x092A8FC1UL, 0x1A7A7C35UL, 0xE811FF36UL, 0x3CDB9BDDUL, 0xCEB018DEUL, 0xDDE0EB2AUL, 0x2F8B6829UL,
        0x82F63B78UL, 0x709DB87BUL, 0x63CD4B8FUL, 0x91A6C88CUL, 0x456CAC67UL, 0xB7072F64UL, 0xA457DC90UL, 0x563C5F93UL,
        0x082F63B7UL, 0xFA44E0B4UL, 0xE9141340UL, 0x1B7F9043UL, 0xCFB5F4A8UL, 0x3DDE77ABUL, 0x2E8E845FUL, 0xDCE5075CUL,
        0x92A8FC17UL, 0x60C37F14UL, 0x73938CE0UL, 0x81F80FE3UL, 0x55326B08UL, 0xA759E80BUL, 0xB4091BFFUL, 0x466298FCUL,
        0x1871A4D8UL, 0xEA1A27DBUL, 0xF94AD42FUL, 0x0B21572CUL, 0xDFEB33C7UL, 0x2D80B0C4UL, 0x3ED04330UL, 0xCCBBC033UL,
        0xA24BB5A6UL, 0x502036A5UL, 0x4370C551UL, 0xB11B4652UL, 0x65D122B9UL, 0x97BAA1BAUL, 0x84EA524EUL, 0x7681D14DUL,
        0x2892ED69UL, 0xDAF96E6AUL, 0xC9A99D9EUL, 0x3BC21E9DUL, 0xEF087A76UL, 0x1D63F975UL, 0x0E330A81UL, 0xFC588982UL,
        0xB21572C9UL, 0x407EF1CAUL, 0x532E023EUL, 0xA145813DUL, 0x758FE5D6UL, 0x87E466D5UL, 0x94B49521UL, 0x66DF1622UL,
        0x38CC2A06UL, 0xCAA7A905UL, 0xD9F75AF1UL, 0x2B9CD9F2UL, 0xFF56BD19UL, 0x0D3D3E1AUL, 0x1E6DCDEEUL, 0xEC064EEDUL,
        0xC38D26C4UL, 0x31E6A5C7UL, 0x22B65633UL, 0xD0DDD530UL, 0x0417B1DBUL, 0xF67C32D8UL, 0xE52CC12CUL, 0x1747422FUL,
        0x49547E0BUL, 0xBB3FFD08UL, 0xA86F0EFCUL, 0x5A048DFFUL, 0x8ECEE914UL, 0x7CA56A17UL, 0x6FF599E3UL, 0x9D9E1AE0UL,
        0xD3D3E1ABUL, 0x21B862A8UL, 0x32E8915CUL, 0xC083125FUL, 0x144976B4UL, 0xE622F5B7UL, 0xF5720643UL, 0x07198540UL,
        0x590AB964UL, 0xAB613A67UL, 0xB831C993UL, 0x4A5A4A90UL, 0x9E902E7BUL, 0x6CFBAD78UL, 0x7FAB5E8CUL, 0x8DC0DD8FUL,
        0xE330A81AUL, 0x115B2B19UL, 0x020BD8EDUL, 0xF0605BEEUL, 0x24AA3F05UL, 0xD6C1BC06UL, 0xC5914FF2UL, 0x37FACCF1UL,
        0x69E9F0D5UL, 0x9B8273D6UL, 0x88D28022UL, 0x7AB90321UL, 0xAE7367CAUL, 0x5C18E4C9UL, 0x4F48173DUL, 0xBD23943EUL,
        0xF36E6F75UL, 0x0105EC76UL, 0x12551F82UL, 0xE03E9C81UL, 0x34F4F86AUL, 0xC69F7B69UL, 0xD5CF889DUL, 0x27A40B9EUL,
        0x79B737BAUL, 0x8BDCB4B9UL, 0x988C474DUL, 0x6AE7C44EUL, 0xBE2DA0A5UL, 0x4C4623A6UL, 0x5F16D052UL, 0xAD7D5351UL,
    };
    return (crc >> BITS_PER_BYTE) ^ CRCTable[byte ^ (crc & BYTE_MAX)];
}

/// Do not forget to apply the output XOR when done
SERARD_PRIVATE TransferCRC transferCRCAdd(const uint32_t crc, const size_t size, const void* const data)
{
    SERARD_ASSERT((data != NULL) || (size == 0U));
    uint32_t      out = crc;
    const uint8_t* p   = (const uint8_t*) data;
    for (size_t i = 0; i < size; i++)
    {
        out = transferCRCAddByte(out, *p);
        ++p;
    }
    return out;
}

// --------------------------------------------- RECEPTION ---------------------------------------------
// TODO: documentation
struct SerardInternalRxSession {
    struct SerardTreeNode      base;
    SerardMicrosecond   transfer_timestamp_usec;
    SerardNodeID        source_node_id;
    size_t              total_payload_size;
    size_t              payload_size;
    uint8_t*            payload;
    // TODO: CRC
    SerardTransferID    transfer_id;
};

// TODO: documentation
struct RxTransferModel {
    SerardMicrosecond   timestamp_usec;
    enum SerardPriority      priority;
    enum SerardTransferKind  transfer_kind;
    SerardPortID        port_id;
    SerardNodeID        source_node_id;
    SerardNodeID        destination_node_id;
    SerardTransferID    transfer_id;
    // bool                end_of_transfer;
    size_t              payload_size;
    const void*         payload;
};

struct FrameHeaderModel
{
    uint8_t version;
    uint8_t priority;
    uint16_t source_node_id;
    uint16_t destination_node_id;
    uint16_t data_specifier_snm;
    uint64_t transfer_id;
    uint32_t frame_index_eot;
    uint16_t user_data;
    uint8_t header_crc16_big_endian[2];
};

struct CobsEncodeState
{
    uint8_t* buffer;
    size_t loc;
    size_t chunk;
};
/// Returns truth if the frame is valid and parsed successfully. False if the frame is not a valid Cyphal/CAN frame.
// SERARD_PRIVATE bool rxTryParseTransfer(const SerardMicrosecond  timestamp_usec,
//                                        const uint8_t* const     payload,
//                                        size_t                   payload_size,
//                                        RxTransferModel* const   out)
// {
//     SERARD_ASSERT(out != NULL);
//     bool valid = false;
//     if (payload_size >= HEADER_SIZE) {
//         SERARD_ASSERT(payload != NULL);
//         out->timestamp_usec = timestamp_usec;
//
//         valid = (payload[0] == HEADER_VERSION);
//
//         // TODO: i doubt this is misra compliant
//         out->priority = (SerardPriority) (payload[OFFSET_PRIORITY] >> OFFSET_PRIORITY_BITS);
//         out->source_node_id = *((SerardNodeID*) &payload[OFFSET_SOURCE_NODE_ID]);
//         out->destination_node_id = *((SerardNodeID*) &payload[OFFSET_DESTINATION_NODE_ID]);
//         out->port_id = *((SerardPortID*) &payload[OFFSET_PORT_ID]);
//         out->transfer_id = *((SerardTransferID*) &payload[OFFSET_TRANSFER_ID]);
//
//         // TODO: validate and parse port id
//         // TODO: CRC
//
//         out->payload_size = payload_size - HEADER_SIZE;
//         out->payload = &payload[HEADER_SIZE];
//
//         // TODO: final validation
//     }
//
//     return valid;
// }
//
// SERARD_PRIVATE void rxInitTransferMetadataFromModel(const RxTransferModel* const     frame,
//                                                        struct SerardTransferMetadata* const out_transfer)
// {
//     SERARD_ASSERT(frame != NULL);
//     SERARD_ASSERT(frame->payload != NULL);
//     SERARD_ASSERT(out_transfer != NULL);
//     out_transfer->priority       = frame->priority;
//     out_transfer->transfer_kind  = frame->transfer_kind;
//     out_transfer->port_id        = frame->port_id;
//     out_transfer->remote_node_id = frame->source_node_id;
//     out_transfer->transfer_id    = frame->transfer_id;
// }
//
// SERARD_PRIVATE int8_t
// rxSubscriptionPredicateOnSession(void* const user_reference,  // NOSONAR Cavl API requires pointer to non-const.
//                                 const struct SerardTreeNode* const node)
// {
//     const SerardNodeID  sought    = *((const SerardNodeID*) user_reference);
//     const SerardNodeID  other     = ((const struct SerardInternalRxSession*) (const void*) node)->source_node_id;
//     static const int8_t NegPos[2] = {-1, +1};
//     // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
//     return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
// }

// SERARD_PRIVATE int8_t rxAcceptTransfer(struct Serard* const                   ins,
//                                        struct SerardRxSubscription* const     subscription,
//                                        const struct SerardRxTransfer* const    transfer,
//                                        struct SerardReassembler* const        reassembler,
//                                        struct SerardRxTransfer* const         out_transfer)
// {
//     SERARD_ASSERT(ins != NULL);
//     SERARD_ASSERT(subscription != NULL);
//     SERARD_ASSERT(transfer != NULL);
//     SERARD_ASSERT(transfer->payload != NULL);
//     SERARD_ASSERT(transfer->transfer_id <= SERARD_TRANSFER_ID_MAX);
//     SERARD_ASSERT(subscription->port_id == transfer->port_id);
//     SERARD_ASSERT((SERARD_NODE_ID_UNSET == transfer->destination_node_id) || (ins->node_id == transfer->destination_node_id));
//     SERARD_ASSERT(out_transfer != NULL);
//
//     int8_t ret = 0;
//     if (transfer->source_node_id <= SERARD_NODE_ID_MAX) {
//         SerardInternalRxSession* const rxs =
//             (SerardInternalRxSession*) ins->memory_allocate(ins, sizeof(SerardInternalRxSession));
//         const SerardTreeNode* const node = cavlSearch(&subscription->sessions,
//                                                       (void *) &transfer->source_node_id, // TODO: should this be void*?
//                                                       &rxSubscriptionPredicateOnSession,
//                                                       &avlTrivialFactory);
//
//         if (rxs != NULL) {
//             rxs->transfer_timestamp_usec = transfer->timestamp_usec;
//             rxs->source_node_id = transfer->source_node_id;
//             rxs->total_payload_size      = 0U;
//             rxs->payload_size            = 0U;
//             rxs->payload                 = NULL;
//             // rxs->calculated_crc          = CRC_INITIAL; // TODO
//             rxs->transfer_id             = transfer->transfer_id;
//         } else {
//             ret = -SERARD_ERROR_OUT_OF_MEMORY;
//         }
//         // There are two possible reasons why the session mserialay not exist: 1. OOM; 2. SOT-miss.
//         if (node != NULL) {
//             SERARD_ASSERT(ret == 0);
//             // TODO: does anything have to be updated?
//             // ret = rxSessionUpdate(ins,
//             //                       subscription->sessions[transfer->source_node_id],
//             //                       transfer,
//             //                       subscription->transfer_id_timeout_usec,
//             //                       subscription->extent,
//             //                       out_transfer);
//         }
//     } else {
//         SERARD_ASSERT(transfer->source_node_id == SERARD_NODE_ID_UNSET);
//         // Anonymous transfers are stateless. No need to update the state machine, just blindly accept it.
//         // We have to copy the data into an allocated storage because the API expects it: the lifetime shall be
//         // independent of the input data and the memory shall be free-able.
//         const size_t payload_size =
//             (subscription->extent < transfer->payload_size) ? subscription->extent : transfer->payload_size;
//         void* const payload = ins->memory_allocate(ins, payload_size);
//         if (payload != NULL) {
//             rxInitTransferMetadataFromModel(transfer, &out_transfer->metadata);
//             out_transfer->timestamp_usec = transfer->timestamp_usec;
//             out_transfer->payload_size   = payload_size;
//             out_transfer->payload        = payload;
//             // Clang-Tidy raises an error recommending the use of memcpy_s() instead.
//             // We ignore it because the safe functions are poorly supported; reliance on them may limit the portability.
//             SERARD_UNUSED(memcpy(payload, transfer->payload, payload_size));  // NOLINT
//             ret = 1;
//         } else {
//             ret = -SERARD_ERROR_OUT_OF_MEMORY;
//         }
//     }
//
//     return ret;
// }

SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnPortID(void* const                        user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const struct SerardTreeNode* const node)
{
    const SerardPortID  sought    = *((const SerardPortID*) user_reference);
    const SerardPortID  other     = ((const struct SerardRxSubscription*) (const void*) node)->port_id;
    static const int8_t NegPos[2] = {-1, +1};
    // Clang-Tidy mistakenly identifies a narrowing cast to int8_t here, which is incorrect.
    return (sought == other) ? 0 : NegPos[sought > other];  // NOLINT no narrowing conversion is taking place here
}


SERARD_PRIVATE int8_t
rxSubscriptionPredicateOnStruct(void* const                        user_reference,  // NOSONAR Cavl API requires pointer to non-const.
                                const struct SerardTreeNode* const node)
{
    return rxSubscriptionPredicateOnPortID(&((struct SerardRxSubscription*) user_reference)->port_id, node);
}

SERARD_PRIVATE uint16_t
// uint16_t
txMakeSessionSpecifier(const enum SerardTransferKind transfer_kind, const SerardPortID port_id)
{
    SERARD_ASSERT(transfer_kind <= SerardTransferKindRequest);

    const uint16_t snm = (transfer_kind == SerardTransferKindMessage) ? 0U : SERVICE_NOT_MESSAGE;
    const uint16_t rnr = (transfer_kind == SerardTransferKindRequest) ? REQUEST_NOT_RESPONSE : 0U;
    const uint16_t id = (uint16_t) port_id;
    const uint16_t out = id | snm | rnr;
    return out;
}

SERARD_PRIVATE uint8_t
txMakeHeader(const struct Serard* const serard, const struct SerardTransferMetadata* const metadata, void* const buffer)
{
    SERARD_ASSERT(metadata != NULL);
    SERARD_ASSERT(buffer != NULL);

    HeaderCRC crc = HEADER_CRC_INITIAL;

    struct FrameHeaderModel* const header = (struct FrameHeaderModel*) buffer; 
    header->version = HEADER_VERSION;
    header->priority = (uint8_t) metadata->priority;
    header->source_node_id = (uint16_t) serard->node_id;
    header->destination_node_id = (uint16_t) metadata->remote_node_id;
    header->data_specifier_snm = txMakeSessionSpecifier(metadata->transfer_kind, metadata->port_id);
    header->transfer_id = (uint64_t) metadata->transfer_id;
    header->frame_index_eot = FRAME_INDEX | END_OF_TRANSFER;
    header->user_data = 0U;

    crc = headerCRCAdd(crc, HEADER_SIZE_NO_CRC, (void *) header);
    header->header_crc16_big_endian[0] = crc >> BITS_PER_BYTE;
    header->header_crc16_big_endian[1] = crc & BYTE_MAX;

    return HEADER_SIZE;
}

SERARD_PRIVATE struct CobsEncodeState
cobsInit(uint8_t* buffer)
{
    SERARD_ASSERT(buffer != NULL);

    struct CobsEncodeState state = {
        .buffer = buffer,
        .loc = 1,
        .chunk = 0,
    };
    return state;
}

SERARD_PRIVATE void
cobsEncode(struct CobsEncodeState *state,
           const uint8_t* const src,
           const size_t len)
{
    SERARD_ASSERT(src != NULL);
    // SERARD_ASSERT(len >= 2);
    // TODO: check if buffer[len - 1] is sentinel?

    size_t cur = 0;
    while (cur < len) {
        state->buffer[state->loc] = src[cur];
        if (src[cur] == COBS_FRAME_DELIMITER) {
            const size_t offset = state->loc - state->chunk;
            SERARD_ASSERT(offset < 255); // TODO
            state->buffer[state->chunk] = (uint8_t) offset;
            state->chunk = state->loc;
        }

        state->loc++;
        cur++;
    }
}

SERARD_PRIVATE size_t cobsEncodingSize(size_t payload)
{
    // COBS encoded frames are bounded by n + ceil(n / 254)
    const size_t overhead = (payload + COBS_OVERHEAD_RATE - 1) / COBS_OVERHEAD_RATE;
    return payload + overhead;
}

// --------------------------------------------- PUBLIC API ---------------------------------------------

struct Serard serardInit(const struct SerardMemoryResource memory_payload,
                         const struct SerardMemoryResource memory_rx_session)
{
    SERARD_ASSERT(memory_payload.allocate != NULL);
    SERARD_ASSERT(memory_payload.deallocate != NULL);
    SERARD_ASSERT(memory_rx_session.allocate != NULL);
    SERARD_ASSERT(memory_rx_session.deallocate != NULL);
    struct Serard serard = {
        .user_reference     = NULL,
        .node_id            = SERARD_NODE_ID_UNSET,
        .memory_payload     = memory_payload,
        .memory_rx_session  = memory_rx_session,
        .rx_subscriptions   = {NULL},
    };

    return serard;
}

int32_t serardTxPush(struct Serard* const                       ins,
                     const struct SerardTransferMetadata* const metadata,
                     const size_t                               payload_size,
                     const void* const                          payload,
                     void* const                                user_reference,
                     const SerardTxEmit                         emitter)
{
    SERARD_ASSERT(ins != NULL);
    SERARD_ASSERT(metadata != NULL);

    size_t header_payload_size = HEADER_SIZE + payload_size + TRANSFER_CRC_SIZE_BYTES;
    size_t max_frame_size = cobsEncodingSize(header_payload_size) + 2U; // 2 bytes extra for frame delimiters
    uint8_t* buffer = ins->memory_payload.allocate(ins->memory_payload.user_reference, max_frame_size);
    if (buffer == NULL) {
        return -SERARD_ERROR_MEMORY;
    }

    size_t buffer_offset = 0;

    buffer[buffer_offset++] = COBS_FRAME_DELIMITER;
    struct CobsEncodeState cobs = cobsInit(&buffer[buffer_offset]);

    uint8_t header[HEADER_SIZE];
    size_t header_size = txMakeHeader(ins, metadata, header);
    cobsEncode(&cobs, header, header_size);

    if (payload_size > 0) {
        cobsEncode(&cobs, payload, payload_size);
    }
    TransferCRC crc = transferCRCAdd(TRANSFER_CRC_INITIAL, payload_size, payload) ^ TRANSFER_CRC_OUTPUT_XOR;
    cobsEncode(&cobs, (void *) &crc, TRANSFER_CRC_SIZE_BYTES);

    uint8_t delimiter = COBS_FRAME_DELIMITER;
    cobsEncode(&cobs, (void *) &delimiter, 1);
    buffer_offset += cobs.loc;

    size_t bytes_transmitted = 0;
    while (bytes_transmitted < buffer_offset) {
        const size_t bytes_left = buffer_offset - bytes_transmitted;
        const uint8_t chunk_size = (bytes_left > 255) ? 255 : ((uint8_t) bytes_left);
        bool out = emitter(user_reference, chunk_size, &buffer[bytes_transmitted]);
        if (!out) {
            ins->memory_payload.deallocate(ins->memory_payload.user_reference, max_frame_size, buffer);
            return 0;
        }

        bytes_transmitted += chunk_size;
    }

    ins->memory_payload.deallocate(ins->memory_payload.user_reference, max_frame_size, buffer);

    return 1; // TODO: what to return?
}

// int8_t serardRxAccept(Serard* const                ins,
//                       SerardReassembler* const     reassembler,
//                       const SerardMicrosecond      timestamp_usec,
//                       size_t* const                inout_payload_size,
//                       const uint8_t* const         payload,
//                       SerardRxTransfer* const      out_transfer,
//                       SerardRxSubscription** const out_subscription)
// {
//     int8_t ret = -SERARD_ERROR_INVALID_ARGUMENT;
//     if ((ins != NULL) && (out_transfer != NULL) && ((payload != NULL) && (*inout_payload_size == 0))) {
//         RxTransferModel model = {0};
//         if (rxTryParseTransfer(timestamp_usec, payload, *inout_payload_size, &model)) {
//             if ((model.destination_node_id == SERARD_NODE_ID_UNSET) || (model.destination_node_id == ins->node_id)) {
//                 // This is the reason the function has a logarithmic time complexity of the number of subscriptions.
//                 // Note also that this one of the two variable-complexity operations in the RX pipeline; the other one
//                 // is memcpy(). Excepting these two cases, the entire RX pipeline contains neither loops nor recursion.
//                 SerardRxSubscription* const sub = (SerardRxSubscription*) (void*) 
//                     cavlSearch(&ins->rx_subscriptions[(size_t) model.transfer_kind],
//                                &model.port_id,
//                                &rxSubscriptionPredicateOnPortID,
//                                NULL);
//                 if (out_subscription != NULL) {
//                     *out_subscription = sub;  // Expose selected instance to the caller.
//                 }
//                 if (sub != NULL) {
//                     SERARD_ASSERT(sub->port_id == model.port_id);
//                     ret = rxAcceptTransfer(ins, sub, &model, reassembler, out_transfer);
//                 } else {
//                     ret = 0;  // No matching subscription.
//                 }
//             } else {
//                 ret = 0; // mis-addressed transfer
//             }
//         } else {
//             ret = 0; // not a valid Cyphal/Serial input packet
//         }
//     }
//
//     SERARD_ASSERT(ret <= 1);
//     return ret;
// }

int8_t serardRxSubscribe(struct Serard* const               ins,
                         const enum SerardTransferKind      transfer_kind,
                         const SerardPortID                 port_id,
                         const size_t                       extent,
                         const SerardMicrosecond            transfer_id_timeout_usec,
                         struct SerardRxSubscription* const out_subscription)
{
    int8_t out = -SERARD_ERROR_ARGUMENT;
    const size_t tk = (size_t) transfer_kind;

    if ((ins != NULL) && (out_subscription != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS)) {
        out = serardRxUnsubscribe(ins, transfer_kind, port_id);
        if (out >= 0) {
            out_subscription->port_id                   = port_id;
            out_subscription->extent                    = extent;
            out_subscription->transfer_id_timeout_usec  = transfer_id_timeout_usec;
            out_subscription->sessions = NULL;

            const struct SerardTreeNode* const node = cavlSearch((struct SerardTreeNode **) &ins->rx_subscriptions[tk],
                                                                 out_subscription,
                                                                 &rxSubscriptionPredicateOnStruct,
                                                                 &avlTrivialFactory);
            SERARD_UNUSED(node);
            SERARD_ASSERT(node == &out_subscription->base);
            out = (out > 0) ? 0 : 1;
        }
    }

    return out;
}

int8_t serardRxUnsubscribe(struct Serard* const          ins,
                           const enum SerardTransferKind transfer_kind,
                           const SerardPortID            port_id)
{
    int8_t ret = -SERARD_ERROR_ARGUMENT;
    const size_t tk = (size_t) transfer_kind;
    if ((ins != NULL) && (tk < SERARD_NUM_TRANSFER_KINDS)) {
        SerardPortID port_id_mutable = port_id;
        struct SerardRxSubscription* const sub = (struct SerardRxSubscription*) (void*)
            cavlSearch((struct SerardTreeNode**) &ins->rx_subscriptions[tk], &port_id_mutable, &rxSubscriptionPredicateOnPortID, NULL);
        if (sub != NULL) {
            cavlRemove((struct SerardTreeNode**) &ins->rx_subscriptions[tk], &sub->base);
            SERARD_ASSERT(sub->port_id == port_id);
            ret = 1;
            // TODO: we should be doing this in O(n), not O(n log n), and without unecessary rotation
            while (sub->sessions != NULL) {
                cavlRemove((struct SerardTreeNode**) &ins->rx_subscriptions[tk], (struct SerardTreeNode*) ins->rx_subscriptions[tk]);
            }
        } else {
            ret = 0;
        }
    }

    return ret;
}
