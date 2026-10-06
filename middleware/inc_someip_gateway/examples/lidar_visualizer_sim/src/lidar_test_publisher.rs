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

use anyhow::Result;
use std::path::Path;
use std::thread::sleep;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const MW_COM_CONFIG_PATH: &str = "examples/lidar_visualizer_sim/config/mw_com_config.json";
const LIDAR_FRAME_INSTANCE_SPECIFIER_ID: &str = "lidar/LidarFrame1";

fn now_ns() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("System clock is before UNIX_EPOCH")
        .as_nanos() as u64
}

pub fn main() -> Result<()> {
    mw_com::initialize(Some(Path::new(MW_COM_CONFIG_PATH)));

    let lidar_frame_instance_specifier =
        mw_com::InstanceSpecifier::try_from(LIDAR_FRAME_INSTANCE_SPECIFIER_ID)
            .expect("LiDAR frame instance specifier creation failed");

    let skeleton =
        lidar_frame_types::LidarFrameInterface::Skeleton::new(&lidar_frame_instance_specifier)
            .expect("LiDAR frame skeleton creation failed");

    let offered_skeleton: lidar_frame_types::LidarFrameInterface::Skeleton<mw_com::skeleton::Offered> =
        skeleton
            .offer_service()
            .expect("Failed offering LiDAR frame skeleton");

    let mut frame_id: u32 = 0;

    loop {
        let mut frame = lidar_frame_types::LidarFrame::default();
        frame.timestamp_ns = now_ns();
        frame.frame_id = frame_id;
        frame.point_count = 128;
        frame.payload_size = 128 * 16;

        offered_skeleton
            .events
            .lidar_frame_
            .send(frame)
            .expect("Failed sending LiDAR frame event");

        println!(
            "[lidar_test_publisher] sent frame_id={} point_count={} payload_size={}",
            frame_id,
            128,
            128 * 16
        );

        frame_id = frame_id.wrapping_add(1);
        sleep(Duration::from_millis(500));
    }
}