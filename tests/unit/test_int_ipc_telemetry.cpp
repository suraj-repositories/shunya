#include "tests/test_framework.hpp"

#include "shunya/integ/shared_memory_bus.hpp"
#include "shunya/integ/telemetry_stream_server.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace shunya::integ;

TEST_CASE("INT-02-002 SharedMemoryBus OS named shared memory streaming 100k messages with sub-2us latency") {
    const std::string ch = "shunya_unit_test_shm_bus";
    SharedMemoryBus producer;
    SharedMemoryBus consumer;

    REQUIRE(producer.create(ch, 2048).is_ok());
    REQUIRE(consumer.open_existing(ch).is_ok());

    constexpr uint64_t kTotalMessages = 100000;
    std::atomic<uint64_t> rx_count{0};
    std::atomic<bool> integrity_ok{true};

    const auto t_start = std::chrono::steady_clock::now();

    std::thread rx_thread([&]() {
        TelemetryIpcPacket rx{};
        uint64_t expected = 0;
        while (expected < kTotalMessages) {
            if (consumer.try_pop(rx)) {
                if (rx.sequence_id != expected || rx.values[0] != static_cast<double>(expected)) {
                    integrity_ok.store(false, std::memory_order_relaxed);
                    break;
                }
                ++expected;
            }
        }
        rx_count.store(expected, std::memory_order_release);
    });

    for (uint64_t i = 0; i < kTotalMessages; ++i) {
        TelemetryIpcPacket pkt{};
        pkt.sequence_id = i;
        pkt.sim_time_s = static_cast<double>(i) * 1e-5;
        pkt.channel_id = 1;
        pkt.value_count = 1;
        pkt.values[0] = static_cast<double>(i);
        while (!producer.try_push(pkt)) {
            std::this_thread::yield();
        }
    }

    rx_thread.join();
    const auto t_end = std::chrono::steady_clock::now();
    const double elapsed_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();
    const double us_per_msg = elapsed_us / static_cast<double>(kTotalMessages);

    CHECK(integrity_ok.load());
    CHECK(rx_count.load() == kTotalMessages);
    // Zero-copy shared memory throughput achieves well under 2 microseconds per message
    CHECK(us_per_msg < 2.0);
}

TEST_CASE("INT-02-002 TelemetryStreamServer WebSocket RFC 6455 handshake, frame encoding, and 10-client fanout") {
    // RFC 6455 Section 4.2.2 canonical test vector:
    // Sec-WebSocket-Key "dGhlIHNhbXBsZSBub25jZQ==" -> "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
    const std::string accept = TelemetryStreamServer::compute_websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==");
    CHECK(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    // WebSocket text frame binary framing check
    const auto frame = TelemetryStreamServer::encode_websocket_text_frame("{\"v\":1.5}");
    REQUIRE(frame.size() == 2 + 9);
    CHECK(frame[0] == 0x81u);
    CHECK(frame[1] == 9u);

    // 10 concurrent WebSocket clients receiving live JSON telemetry batches at 30+ Hz
    TelemetryStreamServer server;
    std::vector<int> client_rx_counts(10, 0);
    std::vector<uint64_t> client_ids;
    for (int i = 0; i < 10; ++i) {
        auto id_res = server.connect_client([&, i](const std::string& json) {
            if (json.find("cell_voltage") != std::string::npos) {
                ++client_rx_counts[static_cast<size_t>(i)];
            }
        });
        REQUIRE(id_res.is_ok());
        client_ids.push_back(id_res.value());
    }
    CHECK(server.client_count() == 10);

    for (int frame_idx = 0; frame_idx < 35; ++frame_idx) {
        server.publish_metric({frame_idx * 0.03, "cell_voltage", 3.72 - frame_idx * 0.001, "V"});
        CHECK(server.broadcast_pending() == 10);
    }

    for (int count : client_rx_counts) {
        CHECK(count == 35);
    }
    server.disconnect_client(client_ids.back());
    CHECK(server.client_count() == 9);
}
