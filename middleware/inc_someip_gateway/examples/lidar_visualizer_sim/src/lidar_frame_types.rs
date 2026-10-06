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

pub const MAX_LIDAR_PAYLOAD_SIZE: usize = 65560;

#[repr(C, align(16))]
#[derive(Clone, Copy)]
pub struct LidarFrame {
    // C++ offset 0
    pub size: usize,

    // C++ data is alignas(std::max_align_t), therefore starts at offset 16.
    pub _padding: [u8; 8],

    // C++ offset 16
    pub data: [u8; MAX_LIDAR_PAYLOAD_SIZE],

    // 16-byte struct alignment rounds total size from 65576 to 65584.
    pub _tail_padding: [u8; 8],
}

impl Default for LidarFrame {
    fn default() -> Self {
        Self {
            size: 0,
            _padding: [0; 8],
            data: [0; MAX_LIDAR_PAYLOAD_SIZE],
            _tail_padding: [0; 8],
        }
    }
}

impl std::fmt::Debug for LidarFrame {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LidarFrame")
            .field("size", &self.size)
            .finish()
    }
}

mw_com::import_interface!(my_LidarFrameInterface, LidarFrameInterface, {
    lidar_frame_: Event<crate::LidarFrame>
});

mw_com::import_type!(my_LidarFrame, crate::LidarFrame);