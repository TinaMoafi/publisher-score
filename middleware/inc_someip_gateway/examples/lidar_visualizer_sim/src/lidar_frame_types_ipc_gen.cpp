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
#include "lidar_frame_types.h"
#include "score/mw/com/impl/rust/bridge_macros.h"

BEGIN_EXPORT_MW_COM_INTERFACE(my_LidarFrameInterface, ::lidar_frame_types::LidarFrameProxy,
                              ::lidar_frame_types::LidarFrameSkeleton)
EXPORT_MW_COM_EVENT(my_LidarFrameInterface, ::lidar_frame_types::LidarFrame, lidar_frame_)
END_EXPORT_MW_COM_INTERFACE()
EXPORT_MW_COM_TYPE(my_LidarFrame, ::lidar_frame_types::LidarFrame)