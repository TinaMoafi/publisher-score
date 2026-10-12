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

#include <cstdint>

#include "score/mw/com/types.h"

namespace lidar_frame_types {

struct LidarFrame {
    std::uint64_t timestamp_ns;
    std::uint32_t frame_id;
    std::uint32_t point_count;
    std::uint32_t payload_size;
};

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