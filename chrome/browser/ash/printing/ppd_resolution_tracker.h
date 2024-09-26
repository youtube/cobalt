// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_TRACKER_H_
#define CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_TRACKER_H_

#include <string>
#include <unordered_map>

#include "chromeos/printing/printer_configuration.h"

namespace ash {

class PpdResolutionState;

class PpdResolutionTracker {
 public:
  PpdResolutionTracker();
  PpdResolutionTracker(PpdResolutionTracker&& other);
  PpdResolutionTracker& operator=(PpdResolutionTracker&& rhs);

  PpdResolutionTracker(const PpdResolutionTracker&) = delete;
  PpdResolutionTracker& operator=(const PpdResolutionTracker&) = delete;

  ~PpdResolutionTracker();

  // Returns true if a |printer_id| is in |printer_state_| and if the printer
  // ppd resolution is not inflight.
  bool IsResolutionComplete(const std::string& printer_id) const;

  // Returns true if the printer PpdReference resolution is currently inflight.
  bool IsResolutionPending(const std::string& printer_id) const;

  // Returns true if the a PpdReference was successfully resolved.
  bool WasResolutionSuccessful(const std::string& printer_id) const;

  // Adds |printer_id| to |printer_state_|. Default state is when the resolution
  // is inflight.
  void MarkResolutionPending(const std::string& printer_id);

  // Store |ppd_reference| and update the resolution state of |printer_id| as
  // successful.
  void MarkResolutionSuccessful(
      const std::string& printer_id,
      const chromeos::Printer::PpdReference& ppd_reference);

  // Update |printer_id|'s resolution state as failed.
  void MarkResolutionFailed(const std::string& printer_id);

  // Store the |usb_manufacturer| for the associated |printer_id|.
  void SetManufacturer(const std::string& printer_id,
                       const std::string& usb_manufacturer);

  // Returns the Usb manufacturer for the associated |printed_id|.
  const std::string& GetManufacturer(const std::string& printer_id) const;

  // Returns the PpdReference for the associated |printer_id|.
  const chromeos::Printer::PpdReference& GetPpdReference(
      const std::string& printer_id) const;

  // Mark the printer as not autconfigurable. It is set when the configuration
  // of IPP USB printer fails because the printer does not meet the IPP
  // Everywhere requirements.
  void MarkPrinterAsNotAutoconfigurable(const std::string& printer_id);

  // Returns true <=> the function above was called for |printer_id|.
  bool IsMarkedAsNotAutoconfigurable(const std::string& printer_id) const;

 private:
  // Returns true if |printer_id| exists in |printer_state_|.
  bool PrinterStateExists(const std::string& printer_id) const;

  std::unordered_map<std::string, PpdResolutionState> printer_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_TRACKER_H_
