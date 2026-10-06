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

#ifndef LIDAR_FRAME_TYPES_H
#define LIDAR_FRAME_TYPES_H

#include <cstddef>

#include "score/mw/com/types.h"
#include "src/serializer/pre_serialized_data.h"

namespace lidar_frame_types {

constexpr std::size_t MAX_LIDAR_PAYLOAD_SIZE = 65560;

/*
 * This must match the object type produced by gatewayd's NullSerializer.
 *
 * gatewayd receives the SOME/IP payload and stores:
 *
 *     size = payload size
 *     data = exact SOME/IP payload bytes
 *
 * For the current LiDAR publisher the expected payload size is 1020 bytes.
 */
using LidarFrame =
    score::someip_gateway::serializer::PreSerializedData<MAX_LIDAR_PAYLOAD_SIZE>;

template <typename Trait>
class LidarFrameInterface : public Trait::Base {
   public:
    using Trait::Base::Base;

    typename Trait::template Event<LidarFrame> lidar_frame_{*this, "lidar_frame"};
};

using LidarFrameProxy = score::mw::com::AsProxy<LidarFrameInterface>;
using LidarFrameSkeleton = score::mw::com::AsSkeleton<LidarFrameInterface>;

}  // namespace lidar_frame_types

#endif  // LIDAR_FRAME_TYPES_H