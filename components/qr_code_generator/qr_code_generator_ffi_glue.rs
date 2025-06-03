// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::CxxVector;
use qr_code::types::{Color, EcLevel, QrError};
use qr_code::{QrCode, Version};
use std::pin::Pin;

// Requires this allow since cxx generates unsafe code.
//
// TODO(crbug.com/1422745): patch upstream cxx to generate compatible code.
#[allow(unsafe_op_in_unsafe_fn)]
#[cxx::bridge(namespace = "qr_code_generator")]
mod ffi {
    extern "Rust" {
        fn generate_qr_code_using_rust(
            data: &[u8],
            min_version: i16,
            out_pixels: Pin<&mut CxxVector<u8>>,
            out_qr_size: &mut usize,
        ) -> bool;
    }
}

/// Translates `data` into a QR code.
///
/// `min_version` can request a minimum QR code version.
fn generate(data: &[u8], min_version: Option<i16>) -> Result<QrCode, QrError> {
    let mut qr_code = QrCode::new(data)?;

    let actual_version = match qr_code.version() {
        Version::Micro(_) => panic!("QrCode::new should not generate micro QR codes"),
        Version::Normal(actual_version) => actual_version,
    };

    match min_version {
        None => (),
        Some(min_version) if actual_version >= min_version => (),
        Some(min_version) => {
            // If `actual_version` < `min_version`, then re-encode using `min_version`
            qr_code = QrCode::with_version(data, Version::Normal(min_version), EcLevel::M)?;
        }
    }

    Ok(qr_code)
}

/// Translates `data` into a QR code.
///
/// `min_version` can request a minimum QR code version (e.g. if the caller
/// requires that the QR code has a certain minimal width and height;  for
/// example, QR code version 5 translates into 37x37 QR pixels).  Setting
/// `min_version` to 0 or less means that the caller doesn't have any QR version
/// requirements.
///
/// On success returns `true` and populates `out_pixels` and `out_qr_size`.  On
/// failure returns `false`.
pub fn generate_qr_code_using_rust(
    data: &[u8],
    min_version: i16,
    mut out_pixels: Pin<&mut CxxVector<u8>>,
    out_qr_size: &mut usize,
) -> bool {
    let min_version = if min_version <= 0 {
        None
    } else {
        assert!(min_version <= 40);
        Some(min_version)
    };

    match generate(data, min_version) {
        Err(_) => false,
        Ok(qr_code) => {
            *out_qr_size = qr_code.width().into();

            assert!(out_pixels.is_empty());
            for color in qr_code.into_colors() {
                let u8_value = match color {
                    Color::Light => 0,
                    Color::Dark => 1,
                };
                out_pixels.as_mut().push(u8_value);
            }

            true
        }
    }
}
