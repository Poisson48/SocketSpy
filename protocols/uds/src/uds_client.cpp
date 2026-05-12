#include "uds_client.h"

namespace socketspy::protocols::uds {

UdsResponse decode_response(const uint8_t* data, uint16_t len) noexcept {
    UdsResponse resp{};
    if (data == nullptr || len < 1) {
        resp.positive = false;
        resp.service  = Service::TesterPresent;
        resp.nrc      = NrcCode::GeneralReject;
        return resp;
    }

    if (data[0] == 0x7Fu) {
        // Negative response: [0x7F, service_id, nrc]
        resp.positive = false;
        resp.service  = (len >= 2) ? static_cast<Service>(data[1]) : Service::TesterPresent;
        resp.nrc      = (len >= 3) ? static_cast<NrcCode>(data[2])  : NrcCode::GeneralReject;
    } else {
        resp.positive = true;
        // Positive response: service byte = request_id + 0x40
        resp.service  = static_cast<Service>(data[0] - 0x40u);
        if (len > 1)
            resp.data.assign(data + 1, data + len);
    }
    return resp;
}

std::string_view service_name(Service s) noexcept {
    switch (s) {
        case Service::DiagnosticSessionControl:        return "DiagnosticSessionControl";
        case Service::ECUReset:                        return "ECUReset";
        case Service::SecurityAccess:                  return "SecurityAccess";
        case Service::CommunicationControl:            return "CommunicationControl";
        case Service::ReadDataByIdentifier:            return "ReadDataByIdentifier";
        case Service::ReadMemoryByAddress:             return "ReadMemoryByAddress";
        case Service::ReadScalingDataByIdentifier:     return "ReadScalingDataByIdentifier";
        case Service::ReadDTCInformation:              return "ReadDTCInformation";
        case Service::WriteDataByIdentifier:           return "WriteDataByIdentifier";
        case Service::InputOutputControlByIdentifier:  return "InputOutputControlByIdentifier";
        case Service::RoutineControl:                  return "RoutineControl";
        case Service::RequestDownload:                 return "RequestDownload";
        case Service::TransferData:                    return "TransferData";
        case Service::RequestTransferExit:             return "RequestTransferExit";
        case Service::TesterPresent:                   return "TesterPresent";
        default:                                       return "UnknownService";
    }
}

std::string_view nrc_name(NrcCode c) noexcept {
    switch (c) {
        case NrcCode::GeneralReject:               return "GeneralReject";
        case NrcCode::ServiceNotSupported:         return "ServiceNotSupported";
        case NrcCode::SubfunctionNotSupported:     return "SubfunctionNotSupported";
        case NrcCode::IncorrectLength:             return "IncorrectLength";
        case NrcCode::ResponsePending:             return "ResponsePending";
        case NrcCode::RequestOutOfRange:           return "RequestOutOfRange";
        case NrcCode::SecurityAccessDenied:        return "SecurityAccessDenied";
        case NrcCode::InvalidKey:                  return "InvalidKey";
        case NrcCode::ExceedNumberOfAttempts:      return "ExceedNumberOfAttempts";
        case NrcCode::RequiredTimeDelayNotExpired: return "RequiredTimeDelayNotExpired";
        case NrcCode::ConditionsNotCorrect:        return "ConditionsNotCorrect";
        default:                                   return "UnknownNRC";
    }
}

std::vector<uint8_t> build_session_control(uint8_t session_type) {
    return {static_cast<uint8_t>(Service::DiagnosticSessionControl), session_type};
}

std::vector<uint8_t> build_tester_present(bool suppress_response) {
    return {static_cast<uint8_t>(Service::TesterPresent),
            static_cast<uint8_t>(suppress_response ? 0x80u : 0x00u)};
}

std::vector<uint8_t> build_read_did(uint16_t did) {
    return {static_cast<uint8_t>(Service::ReadDataByIdentifier),
            static_cast<uint8_t>(did >> 8),
            static_cast<uint8_t>(did & 0xFFu)};
}

std::vector<uint8_t> build_ecu_reset(uint8_t reset_type) {
    return {static_cast<uint8_t>(Service::ECUReset), reset_type};
}

} // namespace socketspy::protocols::uds
