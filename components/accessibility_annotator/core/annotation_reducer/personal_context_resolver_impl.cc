// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace accessibility_annotator {

namespace {

MemoryDataType ToMemoryDataType(
    personal_context::proto::MemoryDataType data_type) {
  switch (data_type) {
    case personal_context::proto::MEMORY_DATA_TYPE_UNSPECIFIED:
      return MemoryDataType::kUnknown;
    case personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL:
      return MemoryDataType::kNameFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_FULL:
      return MemoryDataType::kAddressFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STREET_ADDRESS:
      return MemoryDataType::kAddressStreetAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_CITY:
      return MemoryDataType::kAddressCity;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_STATE:
      return MemoryDataType::kAddressState;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_ZIP:
      return MemoryDataType::kAddressZip;
    case personal_context::proto::MEMORY_DATA_TYPE_ADDRESS_COUNTRY:
      return MemoryDataType::kAddressCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PHONE:
      return MemoryDataType::kPhone;
    case personal_context::proto::MEMORY_DATA_TYPE_EMAIL:
      return MemoryDataType::kEmail;
    case personal_context::proto::MEMORY_DATA_TYPE_COMPANY_NAME:
      return MemoryDataType::kCompanyName;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN:
      return MemoryDataType::kIban;
    case personal_context::proto::MEMORY_DATA_TYPE_IBAN_NICKNAME:
      return MemoryDataType::kIbanNickname;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE:
      return MemoryDataType::kVehicle;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MAKE:
      return MemoryDataType::kVehicleMake;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_MODEL:
      return MemoryDataType::kVehicleModel;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_YEAR:
      return MemoryDataType::kVehicleYear;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_OWNER:
      return MemoryDataType::kVehicleOwner;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_NUMBER:
      return MemoryDataType::kVehiclePlateNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_PLATE_STATE:
      return MemoryDataType::kVehiclePlateState;
    case personal_context::proto::MEMORY_DATA_TYPE_VEHICLE_VIN:
      return MemoryDataType::kVehicleVin;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_FULL:
      return MemoryDataType::kPassportFull;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NAME:
      return MemoryDataType::kPassportName;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_COUNTRY:
      return MemoryDataType::kPassportCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_NUMBER:
      return MemoryDataType::kPassportNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_ISSUE_DATE:
      return MemoryDataType::kPassportIssueDate;
    case personal_context::proto::MEMORY_DATA_TYPE_PASSPORT_EXPIRATION_DATE:
      return MemoryDataType::kPassportExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FULL:
      return MemoryDataType::kFlightReservationFull;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return MemoryDataType::kFlightReservationFlightNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_TICKET_NUMBER:
      return MemoryDataType::kFlightReservationTicketNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return MemoryDataType::kFlightReservationConfirmationCode;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_PASSENGER_NAME:
      return MemoryDataType::kFlightReservationPassengerName;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
      return MemoryDataType::kFlightReservationDepartureAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
      return MemoryDataType::kFlightReservationArrivalAirport;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_DEPARTURE_DATE:
      return MemoryDataType::kFlightReservationDepartureDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_FLIGHT_RESERVATION_ARRIVAL_DATE:
      return MemoryDataType::kFlightReservationArrivalDate;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_FULL:
      return MemoryDataType::kShipmentFull;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_TRACKING_NUMBER:
      return MemoryDataType::kShipmentTrackingNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_ASSOCIATED_ORDER_ID:
      return MemoryDataType::kShipmentAssociatedOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ADDRESS:
      return MemoryDataType::kShipmentDeliveryAddress;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_DELIVERY_ZIP_CODE:
      return MemoryDataType::kShipmentDeliveryZipCode;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_NAME:
      return MemoryDataType::kShipmentCarrierName;
    case personal_context::proto::MEMORY_DATA_TYPE_SHIPMENT_CARRIER_DOMAIN:
      return MemoryDataType::kShipmentCarrierDomain;
    case personal_context::proto::
        MEMORY_DATA_TYPE_SHIPMENT_ESTIMATED_DELIVERY_DATE:
      return MemoryDataType::kShipmentEstimatedDeliveryDate;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_FULL:
      return MemoryDataType::kNationalIdCardFull;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NAME:
      return MemoryDataType::kNationalIdCardName;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_COUNTRY:
      return MemoryDataType::kNationalIdCardCountry;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_NUMBER:
      return MemoryDataType::kNationalIdCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_NATIONAL_ID_CARD_ISSUE_DATE:
      return MemoryDataType::kNationalIdCardIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_NATIONAL_ID_CARD_EXPIRATION_DATE:
      return MemoryDataType::kNationalIdCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_FULL:
      return MemoryDataType::kRedressNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NAME:
      return MemoryDataType::kRedressNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_REDRESS_NUMBER_NUMBER:
      return MemoryDataType::kRedressNumberNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_FULL:
      return MemoryDataType::kKnownTravelerNumberFull;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NAME:
      return MemoryDataType::kKnownTravelerNumberName;
    case personal_context::proto::MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_NUMBER:
      return MemoryDataType::kKnownTravelerNumberNumber;
    case personal_context::proto::
        MEMORY_DATA_TYPE_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return MemoryDataType::kKnownTravelerNumberExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_FULL:
      return MemoryDataType::kDriversLicenseFull;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NAME:
      return MemoryDataType::kDriversLicenseName;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_STATE:
      return MemoryDataType::kDriversLicenseState;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_NUMBER:
      return MemoryDataType::kDriversLicenseNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_DRIVERS_LICENSE_ISSUE_DATE:
      return MemoryDataType::kDriversLicenseIssueDate;
    case personal_context::proto::
        MEMORY_DATA_TYPE_DRIVERS_LICENSE_EXPIRATION_DATE:
      return MemoryDataType::kDriversLicenseExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_FULL:
      return MemoryDataType::kOrderFull;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ID:
      return MemoryDataType::kOrderId;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_ACCOUNT:
      return MemoryDataType::kOrderAccount;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_DATE:
      return MemoryDataType::kOrderDate;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_NAME:
      return MemoryDataType::kOrderMerchantName;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_MERCHANT_DOMAIN:
      return MemoryDataType::kOrderMerchantDomain;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_PRODUCT_NAMES:
      return MemoryDataType::kOrderProductNames;
    case personal_context::proto::MEMORY_DATA_TYPE_ORDER_GRAND_TOTAL:
      return MemoryDataType::kOrderGrandTotal;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NUMBER:
      return MemoryDataType::kCreditCardNumber;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_EXPIRATION_DATE:
      return MemoryDataType::kCreditCardExpirationDate;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_SECURITY_CODE:
      return MemoryDataType::kCreditCardSecurityCode;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NAME_ON_CARD:
      return MemoryDataType::kCreditCardNameOnCard;
    case personal_context::proto::MEMORY_DATA_TYPE_CREDIT_CARD_NICKNAME:
      return MemoryDataType::kCreditCardNickname;
  }
}

std::vector<MemoryEntrySource> ExtractSources(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<MemoryEntrySource> sources;
  for (const personal_context::proto::SourceReference& proto_source :
       proto_result.sources()) {
    if (proto_source.has_gmail()) {
      std::string_view message_url = proto_source.gmail().message_url();
      sources.emplace_back(MemoryEntrySourceType::kGmail,
                           message_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(message_url));
    } else if (proto_source.has_photos()) {
      std::string_view photos_url = proto_source.photos().photos_url();
      sources.emplace_back(MemoryEntrySourceType::kPhotos,
                           photos_url.empty()
                               ? std::nullopt
                               : std::make_optional<std::string>(photos_url));
    }
  }
  return sources;
}

std::vector<EntryMetadata> ExtractMetadata(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  std::vector<EntryMetadata> metadata_list;
  for (const personal_context::proto::Attribute& secondary :
       proto_result.secondary_attributes()) {
    MemoryDataType other_type = MemoryDataType::kUnknown;
    std::u16string other_type_name;
    if (secondary.has_schemaful_key()) {
      other_type = ToMemoryDataType(secondary.schemaful_key());
    } else if (secondary.has_schemaless_key()) {
      other_type_name = base::UTF8ToUTF16(secondary.schemaless_key());
    }
    metadata_list.emplace_back(other_type, other_type_name,
                               base::UTF8ToUTF16(secondary.value()));
  }
  return metadata_list;
}

MemorySearchResult ConvertToMemorySearchResult(
    const personal_context::proto::AtMemorySearchResult& proto_result) {
  MemoryDataType memory_data_type = MemoryDataType::kUnknown;
  std::u16string type_name;
  std::u16string primary_value;
  if (proto_result.has_primary_attribute()) {
    const personal_context::proto::Attribute& primary =
        proto_result.primary_attribute();
    if (primary.has_schemaful_key()) {
      memory_data_type = ToMemoryDataType(primary.schemaful_key());
    } else if (primary.has_schemaless_key()) {
      type_name = base::UTF8ToUTF16(primary.schemaless_key());
    }
    primary_value = base::UTF8ToUTF16(primary.value());
  }

  MemorySearchResult pcontext_result(memory_data_type, type_name, primary_value,
                                     proto_result.relevance_score());
  pcontext_result.sources = ExtractSources(proto_result);
  pcontext_result.metadata_list = ExtractMetadata(proto_result);
  return pcontext_result;
}

std::vector<personal_context::proto::MemoryDataType>
GetSupportedLocalDataTypes() {
  std::vector<personal_context::proto::MemoryDataType> types;
  for (int i = personal_context::proto::MemoryDataType_MIN + 1;
       i <= personal_context::proto::MemoryDataType_MAX; ++i) {
    if (personal_context::proto::MemoryDataType_IsValid(i)) {
      types.push_back(static_cast<personal_context::proto::MemoryDataType>(i));
    }
  }
  return types;
}

}  // namespace

PersonalContextResolverImpl::PersonalContextResolverImpl(
    personal_context::PersonalContextService* personal_context_service,
    const std::string& locale)
    : personal_context_service_(personal_context_service), locale_(locale) {}

PersonalContextResolverImpl::~PersonalContextResolverImpl() = default;

void PersonalContextResolverImpl::Query(std::u16string query,
                                        QueryCallback callback) {
  // Explicitly cancels any in-flight request and invokes its callback with an
  // empty result set. This enforces the contract that only one request can
  // be active at a time.
  if (in_flight_query_callback_) {
    std::move(in_flight_query_callback_)
        .Run(base::unexpected(
            personal_context::ContextMemoryError::FromExecutionError(
                personal_context::ContextMemoryError::ExecutionError::
                    kCancelled)));
  }

  // Cancel any asynchronous operations tied to the previous request.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!personal_context_service_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(
                personal_context::ContextMemoryError::FromExecutionError(
                    personal_context::ContextMemoryError::ExecutionError::
                        kGenericFailure))));
    return;
  }

  in_flight_query_callback_ = std::move(callback);

  personal_context::proto::AtMemoryQueryRequest request_metadata;
  request_metadata.set_input_query(base::UTF16ToUTF8(query));
  request_metadata.set_locale(locale_);
  for (auto type : GetSupportedLocalDataTypes()) {
    request_metadata.add_supported_local_data_types(type);
  }

  personal_context::ContextMemoryRequestOptions options;
  // TODO(crbug.com/525668259): Control this timeout via a Finch parameter once
  // `PersonalContextResolver` is moved to the autofill component.
  options.request_timeout = base::Seconds(30);
  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY,
      request_metadata, options,
      base::BindOnce(&PersonalContextResolverImpl::OnPersonalContextRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PersonalContextResolverImpl::OnPersonalContextRetrieved(
    personal_context::FetchContextResult result) {
  if (!in_flight_query_callback_) {
    return;
  }

  if (!result.response.has_value()) {
    std::move(in_flight_query_callback_)
        .Run(base::unexpected(result.response.error()));
    return;
  }

  personal_context::proto::AtMemoryQueryResponse response;
  if (!response.ParseFromString(result.response.value().value())) {
    std::move(in_flight_query_callback_)
        .Run(base::unexpected(
            personal_context::ContextMemoryError::FromExecutionError(
                personal_context::ContextMemoryError::ExecutionError::
                    kResponseParseError)));
    return;
  }

  std::vector<MemorySearchResult> results;
  for (int i = 0; i < response.results_size(); ++i) {
    const personal_context::proto::AtMemorySearchResult& proto_result =
        response.results(i);
    MemorySearchResult pcontext_result =
        ConvertToMemorySearchResult(proto_result);
    if (!pcontext_result.value.empty()) {
      results.push_back(std::move(pcontext_result));
    }
  }

  std::move(in_flight_query_callback_).Run(std::move(results));
}

}  // namespace accessibility_annotator
