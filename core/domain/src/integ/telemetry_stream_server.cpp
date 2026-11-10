#include "shunya/integ/telemetry_stream_server.hpp"

#include <array>
#include <cstring>
#include <sstream>

namespace shunya::integ {

namespace {

uint32_t rol32(uint32_t value, int bits) noexcept {
    return (value << bits) | (value >> (32 - bits));
}

std::array<uint8_t, 20> sha1_digest(std::string_view input) {
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;

    std::vector<uint8_t> msg(input.begin(), input.end());
    const uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8ULL;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFu));
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            const size_t o = chunk + static_cast<size_t>(i * 4);
            w[i] = (static_cast<uint32_t>(msg[o]) << 24) |
                   (static_cast<uint32_t>(msg[o + 1]) << 16) |
                   (static_cast<uint32_t>(msg[o + 2]) << 8) |
                   static_cast<uint32_t>(msg[o + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0, k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const uint32_t temp = rol32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol32(b, 30);
            b = a;
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> out{};
    const uint32_t hs[5]{h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        out[static_cast<size_t>(i * 4 + 0)] = static_cast<uint8_t>((hs[i] >> 24) & 0xFFu);
        out[static_cast<size_t>(i * 4 + 1)] = static_cast<uint8_t>((hs[i] >> 16) & 0xFFu);
        out[static_cast<size_t>(i * 4 + 2)] = static_cast<uint8_t>((hs[i] >> 8) & 0xFFu);
        out[static_cast<size_t>(i * 4 + 3)] = static_cast<uint8_t>(hs[i] & 0xFFu);
    }
    return out;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < len) ? data[i + 1] : 0u;
        const uint32_t b2 = (i + 2 < len) ? data[i + 2] : 0u;
        const uint32_t tri = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(kAlphabet[(tri >> 18) & 0x3Fu]);
        out.push_back(kAlphabet[(tri >> 12) & 0x3Fu]);
        out.push_back((i + 1 < len) ? kAlphabet[(tri >> 6) & 0x3Fu] : '=');
        out.push_back((i + 2 < len) ? kAlphabet[tri & 0x3Fu] : '=');
    }
    return out;
}

} // namespace

core::base::Result<uint64_t> TelemetryStreamServer::connect_client(FrameCallback on_frame) {
    if (!on_frame) {
        return core::base::Error{491, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Telemetry client callback cannot be null", ""};
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t id = next_client_id_++;
    clients_[id] = std::move(on_frame);
    return id;
}

void TelemetryStreamServer::disconnect_client(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(client_id);
}

void TelemetryStreamServer::publish_metric(const TelemetryFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_frames_.push_back(frame);
}

size_t TelemetryStreamServer::broadcast_pending() {
    std::vector<TelemetryFrame> batch;
    std::vector<FrameCallback> recipients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_frames_.empty()) return 0;
        batch.swap(pending_frames_);
        recipients.reserve(clients_.size());
        for (const auto& [id, cb] : clients_) {
            recipients.push_back(cb);
        }
    }

    const std::string json = format_json_packet(batch);
    for (const auto& cb : recipients) {
        cb(json);
    }
    ++total_broadcasts_;
    return recipients.size();
}

std::string TelemetryStreamServer::format_json_packet(const std::vector<TelemetryFrame>& frames) {
    std::ostringstream os;
    os << "{\"metrics\":[";
    for (size_t i = 0; i < frames.size(); ++i) {
        if (i > 0) os << ",";
        os << "{\"t\":" << frames[i].timestamp_s
           << ",\"probe\":\"" << frames[i].probe_name << "\""
           << ",\"val\":" << frames[i].value
           << ",\"unit\":\"" << frames[i].unit << "\"}";
    }
    os << "]}";
    return os.str();
}

std::vector<uint8_t> TelemetryStreamServer::encode_websocket_text_frame(std::string_view payload) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81u); // FIN=1, opcode=0x1 (text)
    const size_t len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(126u);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFFu));
        frame.push_back(static_cast<uint8_t>(len & 0xFFu));
    } else {
        frame.push_back(127u);
        for (int b = 7; b >= 0; --b) {
            frame.push_back(static_cast<uint8_t>((static_cast<uint64_t>(len) >> (b * 8)) & 0xFFu));
        }
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

std::string TelemetryStreamServer::compute_websocket_accept_key(std::string_view client_sec_key) {
    static constexpr std::string_view kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined(client_sec_key);
    combined.append(kWsGuid);
    const auto digest = sha1_digest(combined);
    return base64_encode(digest.data(), digest.size());
}

size_t TelemetryStreamServer::client_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

} // namespace shunya::integ
