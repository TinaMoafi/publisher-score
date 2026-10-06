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

use std::path::Path;
use std::thread::sleep;
use std::time::Duration;

const MW_COM_CONFIG_PATH: &str =
    "examples/lidar_visualizer_sim/config/mw_com_config.json";

const LIDAR_FRAME_INSTANCE_SPECIFIER_ID: &str =
    "lidar/LidarFrame1";

const SERVICE_DISCOVERY_SLEEP_DURATION: Duration =
    Duration::from_millis(500);

// -----------------------------------------------------------------------------
// LiDAR SOME/IP payload layout
//
// Header:
//   uint32 ray_count        4 bytes
//   double frame_timestamp  8 bytes
//
// Per ray:
//   double ray_timestamp    8 bytes
//   uint32 ray_id           4 bytes
//   float x                 4 bytes
//   float y                 4 bytes
//   float z                 4 bytes
//   float intensity         4 bytes
//
// Header = 12 bytes
// Ray    = 28 bytes
//
// For 36 rays:
//   12 + (36 * 28) = 1020 bytes
// -----------------------------------------------------------------------------

const HEADER_SIZE: usize = 12;
const RAY_SIZE: usize = 28;

#[derive(Debug)]
struct LidarRay {
    timestamp: f64,
    id: u32,
    x: f32,
    y: f32,
    z: f32,
    intensity: f32,
}

fn read_u32_le(data: &[u8], offset: usize) -> Option<u32> {
    let bytes: [u8; 4] = data.get(offset..offset + 4)?.try_into().ok()?;
    Some(u32::from_le_bytes(bytes))
}

fn read_f32_le(data: &[u8], offset: usize) -> Option<f32> {
    let bytes: [u8; 4] = data.get(offset..offset + 4)?.try_into().ok()?;
    Some(f32::from_le_bytes(bytes))
}

fn read_f64_le(data: &[u8], offset: usize) -> Option<f64> {
    let bytes: [u8; 8] = data.get(offset..offset + 8)?.try_into().ok()?;
    Some(f64::from_le_bytes(bytes))
}

fn deserialize_lidar_payload(data: &[u8], frame_number: u64) {
    if data.len() < HEADER_SIZE {
        eprintln!(
            "[sub] frame={} ERROR: payload too short: {} bytes",
            frame_number,
            data.len()
        );
        return;
    }

    let ray_count = match read_u32_le(data, 0) {
        Some(value) => value,
        None => {
            eprintln!("[sub] failed to decode ray_count");
            return;
        }
    };

    let frame_timestamp = match read_f64_le(data, 4) {
        Some(value) => value,
        None => {
            eprintln!("[sub] failed to decode frame timestamp");
            return;
        }
    };

    let expected_size =
        HEADER_SIZE + (ray_count as usize * RAY_SIZE);

    if data.len() < expected_size {
        eprintln!(
            "[sub] frame={} ERROR: truncated payload: \
             ray_count={} expected={}B received={}B",
            frame_number,
            ray_count,
            expected_size,
            data.len()
        );
        return;
    }

    println!();
    println!("============================================================");
    println!("[sub] frame={}", frame_number);
    println!("      payload_size={}", data.len());
    println!("      ray_count={}", ray_count);
    println!("      frame_timestamp={:.6}", frame_timestamp);
    println!("------------------------------------------------------------");

    for i in 0..ray_count as usize {
        let base = HEADER_SIZE + i * RAY_SIZE;

        let ray = LidarRay {
            timestamp: match read_f64_le(data, base) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} timestamp", i);
                    return;
                }
            },

            id: match read_u32_le(data, base + 8) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} id", i);
                    return;
                }
            },

            x: match read_f32_le(data, base + 12) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} x", i);
                    return;
                }
            },

            y: match read_f32_le(data, base + 16) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} y", i);
                    return;
                }
            },

            z: match read_f32_le(data, base + 20) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} z", i);
                    return;
                }
            },

            intensity: match read_f32_le(data, base + 24) {
                Some(v) => v,
                None => {
                    eprintln!("[sub] failed decoding ray {} intensity", i);
                    return;
                }
            },
        };

        println!(
            "      ray[{:02}] ts={:.6} id={} \
             x={:.3} y={:.3} z={:.3} intensity={:.3}",
            i,
            ray.timestamp,
            ray.id,
            ray.x,
            ray.y,
            ray.z,
            ray.intensity
        );
    }

    println!("============================================================");
}

fn main() {
    mw_com::initialize(Some(Path::new(MW_COM_CONFIG_PATH)));

    let lidar_frame_instance_specifier =
        mw_com::InstanceSpecifier::try_from(
            LIDAR_FRAME_INSTANCE_SPECIFIER_ID,
        )
        .expect("LiDAR frame instance specifier creation failed");

    println!(
        "[sub] looking for {}",
        LIDAR_FRAME_INSTANCE_SPECIFIER_ID
    );

    loop {
        let handles = loop {
            let handles =
                mw_com::proxy::find_service(
                    lidar_frame_instance_specifier.clone(),
                )
                .expect(
                    "LiDAR frame instance specifier resolution failed",
                );

            if !handles.is_empty() {
                break handles;
            }

            println!("[sub] LiDAR service not found, retrying...");

            sleep(SERVICE_DISCOVERY_SLEEP_DURATION);
        };

        println!("[sub] LiDAR service found.");

        let lidar_frame_types::LidarFrameInterface::Proxy {
            lidar_frame_,
        } =
            lidar_frame_types::LidarFrameInterface::Proxy::new(
                &handles[0],
            )
            .expect("Failed to create LiDAR frame proxy");

        let subscribed_lidar_frame =
            lidar_frame_
                .subscribe(10)
                .expect(
                    "Failed to subscribe to LiDAR frame event",
                );

        println!(
            "[sub] subscribed to lidar/LidarFrame1::lidar_frame"
        );

        let mut frame_number: u64 = 0;

        loop {
            if let Some(frame) =
                subscribed_lidar_frame.get_new_sample()
            {
                frame_number += 1;

                let payload_size = frame.size;

                if payload_size
                    > lidar_frame_types::MAX_LIDAR_PAYLOAD_SIZE
                {
                    eprintln!(
                        "[sub] frame={} ERROR: invalid payload size {}",
                        frame_number,
                        payload_size
                    );

                    continue;
                }

                let payload =
                    &frame.data[..payload_size];

                deserialize_lidar_payload(
                    payload,
                    frame_number,
                );
            }

            sleep(Duration::from_millis(10));
        }
    }
}