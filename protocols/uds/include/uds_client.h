#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace socketspy::protocols::uds {

// UDS service IDs (ISO 14229-1)
enum class Service : uint8_t {
    DiagnosticSessionControl   = 0x10,
    ECUReset                   = 0x11,
    SecurityAccess             = 0x27,
    CommunicationControl       = 0x28,
    ReadDataByIdentifier       = 0x22,
    ReadMemoryByAddress        = 0x23,
    ReadScalingDataByIdentifier= 0x24,
    ReadDTCInformation         = 0x19,
    WriteDataByIdentifier      = 0x2E,
    InputOutputControlByIdentifier = 0x2F,
    RoutineControl             = 0x31,
    RequestDownload            = 0x34,
    TransferData               = 0x36,
    RequestTransferExit        = 0x37,
    TesterPresent              = 0x3E,
};

// Negative response codes
enum class NrcCode : uint8_t {
    GeneralReject              = 0x10,
    ServiceNotSupported        = 0x11,
    SubfunctionNotSupported    = 0x12,
    IncorrectLength            = 0x13,
    ResponsePending            = 0x78,
    RequestOutOfRange          = 0x31,
    SecurityAccessDenied       = 0x33,
    InvalidKey                 = 0x35,
    ExceedNumberOfAttempts     = 0x36,
    RequiredTimeDelayNotExpired= 0x37,
    ConditionsNotCorrect       = 0x22,
};

struct UdsResponse {
    bool     positive;
    Service  service;
    NrcCode  nrc;          // valid only if !positive
    std::vector<uint8_t> data;
};

// Decode a raw UDS response frame.
UdsResponse decode_response(const uint8_t* data, uint16_t len) noexcept;

std::string_view service_name(Service s) noexcept;
std::string_view nrc_name(NrcCode c) noexcept;

// Build common UDS requests
std::vector<uint8_t> build_session_control(uint8_t session_type);
std::vector<uint8_t> build_tester_present(bool suppress_response = true);
std::vector<uint8_t> build_read_did(uint16_t did);
std::vector<uint8_t> build_ecu_reset(uint8_t reset_type = 0x01);

} // namespace socketspy::protocols::uds
