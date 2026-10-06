/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "lidar_frame_types.h"
#include "score/mw/com/runtime.h"

using lidar_frame_types::LidarFrameProxy;

using namespace std::chrono_literals;

namespace {

constexpr const char* kLidarFrameInstanceSpecifier = "lidar/LidarFrame1";

constexpr std::uint16_t kMaxSamplesCount = 10U;
constexpr std::size_t kBytesToDump = 64U;

}  // namespace

int main(int argc, const char* argv[]) {
    std::cout
        << "[sub] looking for "
        << kLidarFrameInstanceSpecifier
        << std::endl;

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    const auto instance_specifier =
        score::mw::com::InstanceSpecifier::Create(
            std::string{kLidarFrameInstanceSpecifier})
            .value();

    while (true) {
        auto handles_result =
            LidarFrameProxy::FindService(instance_specifier);

        if (!handles_result.has_value()) {
            std::cerr
                << "[sub] FindService failed, retrying..."
                << std::endl;

            std::this_thread::sleep_for(500ms);
            continue;
        }

        if (handles_result.value().empty()) {
            std::cout
                << "[sub] LiDAR service not found, retrying..."
                << std::endl;

            std::this_thread::sleep_for(500ms);
            continue;
        }

        std::cout
            << "[sub] LiDAR service found."
            << std::endl;

        auto proxy_result =
            LidarFrameProxy::Create(
                handles_result.value().front());

        if (!proxy_result.has_value()) {
            std::cerr
                << "[sub] Failed to create LiDAR proxy"
                << std::endl;

            return 1;
        }

        auto proxy = std::move(proxy_result).value();

        proxy.lidar_frame_.Subscribe(kMaxSamplesCount);

        std::cout
            << "[sub] subscribed to "
            << kLidarFrameInstanceSpecifier
            << "::lidar_frame"
            << std::endl;

        std::uint64_t frame_number = 0U;

        while (true) {
            proxy.lidar_frame_.GetNewSamples(
                [&](auto sample) {
                    ++frame_number;

                    /*
                     * IMPORTANT:
                     *
                     * For now we intentionally do NOT use:
                     *
                     *     sample->size
                     *     sample->data
                     *
                     * We first inspect the exact bytes exposed by the
                     * middleware proxy.
                     */
                    const auto* raw =
                        reinterpret_cast<const std::uint8_t*>(
                            &(*sample));

                    std::cout
                        << "\n============================================================\n"
                        << "[sub-debug] frame="
                        << frame_number
                        << "\n"
                        << "[sub-debug] object address="
                        << static_cast<const void*>(raw)
                        << "\n"
                        << "[sub-debug] first "
                        << kBytesToDump
                        << " bytes:\n";

                    for (std::size_t i = 0U;
                         i < kBytesToDump;
                         ++i) {

                        if ((i % 16U) == 0U) {
                            std::cout
                                << "  "
                                << std::setw(4)
                                << std::setfill('0')
                                << std::hex
                                << i
                                << ": ";
                        }

                        std::cout
                            << std::setw(2)
                            << std::setfill('0')
                            << std::hex
                            << static_cast<unsigned>(raw[i])
                            << " ";

                        if ((i % 16U) == 15U) {
                            std::cout << "\n";
                        }
                    }

                    std::cout
                        << std::dec
                        << std::setfill(' ')
                        << "============================================================"
                        << std::endl;
                },
                kMaxSamplesCount);

            std::this_thread::sleep_for(20ms);
        }
    }

    return 0;
}