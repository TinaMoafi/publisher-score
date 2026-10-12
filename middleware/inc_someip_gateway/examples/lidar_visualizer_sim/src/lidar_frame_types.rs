// *******************************************************************************
// Copyright (c) 2025 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// <https://www.apache.org/licenses/LICENSE-2.0>
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

///! This is the "generated" part for the ipc_bridge proxy. Its main purpose is to provide the imports
///! of the type- and name-dependent part of the FFI and create the respective user-facing objects.

#[repr(C)]
#[derive(Default, Debug, Clone, Copy)]
pub struct LidarFrame {
    pub timestamp_ns: u64,
    pub frame_id: u32,
    pub point_count: u32,
    pub payload_size: u32,
}

mw_com::import_interface!(my_LidarFrameInterface, LidarFrameInterface, {
    lidar_frame_: Event<crate::LidarFrame>
});

mw_com::import_type!(my_LidarFrame, crate::LidarFrame);