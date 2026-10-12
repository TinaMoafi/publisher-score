/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/generic_proxy_event.h"
#include "score/mw/com/runtime.h"

using namespace std::chrono_literals;

namespace
{

constexpr const char* kInstanceSpecifier = "lidar/LidarFrame1";
constexpr const char* kEventName = "lidar_frame";

constexpr std::size_t kExpectedPayloadSize = 1020U;
constexpr std::uint32_t kExpectedRayCount = 36U;
constexpr std::size_t kMaxSamples = 10U;

void PrintBytes(
    const std::byte* data,
    std::size_t count,
    const char* label)
{
    std::cout << label;

    for (std::size_t i = 0U; i < count; ++i)
    {
        const auto value =
            std::to_integer<unsigned int>(data[i]);

        std::cout
            << " "
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << value;
    }

    std::cout
        << std::dec
        << std::setfill(' ')
        << std::endl;
}

}  // namespace

int main(int argc, const char* argv[])
{
    std::cout
        << "[ipc-verifier] starting"
        << std::endl;

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    const auto instance_specifier =
        score::mw::com::InstanceSpecifier::Create(
            std::string{kInstanceSpecifier})
            .value();

    while (true)
    {
        auto handles_result =
            score::mw::com::impl::GenericProxy::FindService(
                instance_specifier);

        if (!handles_result.has_value())
        {
            std::cerr
                << "[ipc-verifier] FindService failed, retrying..."
                << std::endl;

            std::this_thread::sleep_for(500ms);
            continue;
        }

        auto handles =
            std::move(handles_result).value();

        if (handles.empty())
        {
            std::cout
                << "[ipc-verifier] service not found, retrying..."
                << std::endl;

            std::this_thread::sleep_for(500ms);
            continue;
        }

        std::cout
            << "[ipc-verifier] service found"
            << std::endl;

        auto proxy_result =
            score::mw::com::impl::GenericProxy::Create(
                handles.front());

        if (!proxy_result.has_value())
        {
            std::cerr
                << "[ipc-verifier] GenericProxy::Create failed"
                << std::endl;

            return 1;
        }

        auto proxy =
            std::move(proxy_result).value();

        auto& events =
            proxy.GetEvents();

        auto event_it =
            events.find(kEventName);

        if (event_it == events.cend())
        {
            std::cerr
                << "[ipc-verifier] event '"
                << kEventName
                << "' not found"
                << std::endl;

            return 1;
        }

        auto& event =
            event_it->second;

        const std::size_t sample_size =
            event.GetSampleSize();

        std::cout
            << "[ipc-verifier] GenericProxy event found"
            << std::endl;

        std::cout
            << "[ipc-verifier] event storage sample size="
            << sample_size
            << " bytes"
            << std::endl;

        std::cout
            << "[ipc-verifier] serialized-format="
            << (event.HasSerializedFormat() ? "true" : "false")
            << std::endl;

        auto subscribe_result =
            event.Subscribe(kMaxSamples);

        if (!subscribe_result.has_value())
        {
            std::cerr
                << "[ipc-verifier] Subscribe failed"
                << std::endl;

            return 1;
        }

        std::cout
            << "[ipc-verifier] subscribed to "
            << kEventName
            << std::endl;

        std::uint64_t frame_number = 0U;

        while (true)
        {
            auto samples_result =
                event.GetNewSamples(
                    [&](score::mw::com::impl::SamplePtr<void> sample) noexcept
                    {
                        ++frame_number;

                        const void* raw =
                            sample.Get();

                        if (raw == nullptr)
                        {
                            std::cerr
                                << "[ipc-verifier] null sample"
                                << std::endl;
                            return;
                        }

                        const auto* bytes =
                            static_cast<const std::byte*>(raw);

                        std::cout
                            << "\n[ipc-verifier] ===== frame "
                            << frame_number
                            << " ====="
                            << std::endl;

                        PrintBytes(
                            bytes,
                            32U,
                            "[ipc-verifier] slot first 32 bytes:");

                        /*
                         * Search the complete LoLa sample storage for:
                         *
                         * uint32_t ray_count == 36
                         *
                         * Little endian representation:
                         *
                         *     24 00 00 00
                         *
                         * We intentionally search byte-by-byte because we
                         * currently do not assume any wrapper offset.
                         */
                        bool found = false;
                        std::size_t payload_offset = 0U;

                        for (std::size_t offset = 0U;
                             offset + sizeof(std::uint32_t) <= sample_size;
                             ++offset)
                        {
                            std::uint32_t value = 0U;

                            std::memcpy(
                                &value,
                                bytes + offset,
                                sizeof(value));

                            if (value == kExpectedRayCount)
                            {
                                /*
                                 * A complete 1020-byte LiDAR payload must fit
                                 * from this candidate offset.
                                 */
                                if (offset + kExpectedPayloadSize <= sample_size)
                                {
                                    found = true;
                                    payload_offset = offset;
                                    break;
                                }
                            }
                        }

                        if (!found)
                        {
                            std::cerr
                                << "[ipc-verifier] FAIL: ray_count=36 "
                                   "not found in sample storage"
                                << std::endl;

                            return;
                        }

                        std::cout
                            << "[ipc-verifier] candidate LiDAR payload offset="
                            << payload_offset
                            << std::endl;

                        const std::byte* payload =
                            bytes + payload_offset;

                        PrintBytes(
                            payload,
                            32U,
                            "[ipc-verifier] candidate payload first 32 bytes:");

                        std::uint32_t ray_count = 0U;

                        std::memcpy(
                            &ray_count,
                            payload,
                            sizeof(ray_count));

                        std::cout
                            << "[ipc-verifier] candidate ray_count="
                            << ray_count
                            << std::endl;

                        if (ray_count != kExpectedRayCount)
                        {
                            std::cerr
                                << "[ipc-verifier] FAIL: candidate ray_count "
                                   "is invalid"
                                << std::endl;

                            return;
                        }

                        double frame_timestamp = 0.0;

                        std::memcpy(
                            &frame_timestamp,
                            payload + sizeof(std::uint32_t),
                            sizeof(frame_timestamp));

                        std::cout
                            << std::setprecision(17)
                            << "[ipc-verifier] candidate frame_timestamp="
                            << frame_timestamp
                            << std::endl;

                        std::cout
                            << "[ipc-verifier] PASS: found possible "
                            << kExpectedPayloadSize
                            << "-byte LiDAR payload at offset "
                            << payload_offset
                            << std::endl;
                    },
                    kMaxSamples);

            if (!samples_result.has_value())
            {
                std::cerr
                    << "[ipc-verifier] GetNewSamples failed"
                    << std::endl;
            }

            std::this_thread::sleep_for(20ms);
        }
    }
}