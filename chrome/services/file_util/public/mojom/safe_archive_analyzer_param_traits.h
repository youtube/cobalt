// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included param traits file, so no include guard.
// Disabling the presubmit warning with:
//   no-include-guard-because-multiply-included

#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/ipc_protobuf_message_macros.h"
#include "components/safe_browsing/buildflags.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_protobuf_utils.h"

#if !BUILDFLAG(FULL_SAFE_BROWSING)
#error BUILDFLAG(FULL_SAFE_BROWSING) should be set.
#endif

IPC_ENUM_TRAITS_VALIDATE(
    safe_browsing::ClientDownloadRequest_DownloadType,
    safe_browsing::ClientDownloadRequest_DownloadType_IsValid(value))

IPC_ENUM_TRAITS_MAX_VALUE(safe_browsing::ArchiveAnalysisResult,
                          safe_browsing::ArchiveAnalysisResult::kMaxValue)

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(safe_browsing::ClientDownloadRequest_Digests)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(sha256)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(sha1)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(md5)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_CertificateChain_Element)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(certificate)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_CertificateChain)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(element)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_SignatureInfo)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(certificate_chain)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(trusted)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(signed_data)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_PEImageHeaders_DebugData)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(directory_entry)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(raw_data)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_PEImageHeaders)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(dos_header)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(file_header)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(optional_headers32)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(optional_headers64)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(section_header)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(export_section_data)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(debug_data)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_MachOHeaders_LoadCommand)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(command_id)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(command)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_MachOHeaders)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(mach_header)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(load_commands)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_ImageHeaders)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(pe_headers)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(mach_o_headers)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_ArchivedBinary)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(file_path)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(download_type)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(digests)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(length)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(signature)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_COMPLEX_MEMBER(image_headers)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(is_encrypted)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(is_executable)
  IPC_PROTOBUF_MESSAGE_TRAITS_OPTIONAL_FUNDAMENTAL_MEMBER(is_archive)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_PROTOBUF_MESSAGE_TRAITS_BEGIN(
    safe_browsing::ClientDownloadRequest_DetachedCodeSignature)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(file_name)
  IPC_PROTOBUF_MESSAGE_TRAITS_REPEATED_COMPLEX_MEMBER(contents)
IPC_PROTOBUF_MESSAGE_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(
    safe_browsing::EncryptionInfo::PasswordStatus,
    safe_browsing::EncryptionInfo::PasswordStatus::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(safe_browsing::EncryptionInfo)
  IPC_STRUCT_TRAITS_MEMBER(is_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(password_status)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(safe_browsing::ArchiveAnalyzerResults)
  IPC_STRUCT_TRAITS_MEMBER(success)
  IPC_STRUCT_TRAITS_MEMBER(has_executable)
  IPC_STRUCT_TRAITS_MEMBER(has_archive)
  IPC_STRUCT_TRAITS_MEMBER(archived_binary)
  IPC_STRUCT_TRAITS_MEMBER(archived_archive_filenames)
#if BUILDFLAG(IS_MAC)
  IPC_STRUCT_TRAITS_MEMBER(signature_blob)
  IPC_STRUCT_TRAITS_MEMBER(detached_code_signatures)
#endif  // BUILDFLAG(IS_MAC)
  IPC_STRUCT_TRAITS_MEMBER(file_count)
  IPC_STRUCT_TRAITS_MEMBER(directory_count)
  IPC_STRUCT_TRAITS_MEMBER(analysis_result)
  IPC_STRUCT_TRAITS_MEMBER(encryption_info)
IPC_STRUCT_TRAITS_END()
