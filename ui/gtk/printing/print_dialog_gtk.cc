// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/printing/print_dialog_gtk.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
#include "ui/aura/window.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/printing/printing_gtk_util.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/mojom/print.mojom.h"  // nogncheck
#endif

using printing::PageRanges;
using printing::PrintSettings;

namespace {

#if BUILDFLAG(USE_CUPS)
// CUPS Duplex attribute and values.
const char kCUPSDuplex[] = "cups-Duplex";
const char kDuplexNone[] = "None";
const char kDuplexTumble[] = "DuplexTumble";
const char kDuplexNoTumble[] = "DuplexNoTumble";
#endif

constexpr int kPaperSizeTresholdMicrons = 100;
constexpr int kMicronsInMm = 1000;

// Checks whether |gtk_paper_size| can be used to represent user selected media.
// In fuzzy match mode checks that paper sizes are "close enough" (less than
// 1mm difference). In the exact mode, looks for the paper with the same PPD
// name and "close enough" size.
bool PaperSizeMatch(GtkPaperSize* gtk_paper_size,
                    const PrintSettings::RequestedMedia& media,
                    bool fuzzy_match) {
  if (!gtk_paper_size)
    return false;

  gfx::Size paper_size_microns(
      static_cast<int>(gtk_paper_size_get_width(gtk_paper_size, GTK_UNIT_MM) *
                           kMicronsInMm +
                       0.5),
      static_cast<int>(gtk_paper_size_get_height(gtk_paper_size, GTK_UNIT_MM) *
                           kMicronsInMm +
                       0.5));
  int diff = std::max(
      std::abs(paper_size_microns.width() - media.size_microns.width()),
      std::abs(paper_size_microns.height() - media.size_microns.height()));
  bool close_enough = diff <= kPaperSizeTresholdMicrons;
  if (fuzzy_match)
    return close_enough;

  return close_enough && !media.vendor_id.empty() &&
         media.vendor_id == gtk_paper_size_get_ppd_name(gtk_paper_size);
}

// Looks up a paper size matching (in terms of PaperSizeMatch) the user selected
// media in the paper size list reported by GTK. Returns nullptr if there's no
// match found.
GtkPaperSize* FindPaperSizeMatch(GList* gtk_paper_sizes,
                                 const PrintSettings::RequestedMedia& media) {
  GtkPaperSize* first_fuzzy_match = nullptr;
  for (GList* p = gtk_paper_sizes; p && p->data; p = g_list_next(p)) {
    GtkPaperSize* gtk_paper_size = static_cast<GtkPaperSize*>(p->data);
    if (PaperSizeMatch(gtk_paper_size, media, false))
      return gtk_paper_size;

    if (!first_fuzzy_match && PaperSizeMatch(gtk_paper_size, media, true))
      first_fuzzy_match = gtk_paper_size;
  }
  return first_fuzzy_match;
}

class StickyPrintSettingGtk {
 public:
  StickyPrintSettingGtk() : last_used_settings_(gtk_print_settings_new()) {}

  StickyPrintSettingGtk(const StickyPrintSettingGtk&) = delete;
  StickyPrintSettingGtk& operator=(const StickyPrintSettingGtk&) = delete;

  ~StickyPrintSettingGtk() {
    NOTREACHED();  // Intended to be used with base::NoDestructor.
  }

  GtkPrintSettings* settings() { return last_used_settings_; }

  void SetLastUsedSettings(GtkPrintSettings* settings) {
    DCHECK(last_used_settings_);
    g_object_unref(last_used_settings_);
    last_used_settings_ = gtk_print_settings_copy(settings);
  }

 private:
  raw_ptr<GtkPrintSettings> last_used_settings_;
};

StickyPrintSettingGtk& GetLastUsedSettings() {
  static base::NoDestructor<StickyPrintSettingGtk> settings;
  return *settings;
}

// Helper class to track GTK printers.
class GtkPrinterList {
 public:
  GtkPrinterList() { gtk_enumerate_printers(SetPrinter, this, nullptr, TRUE); }

  ~GtkPrinterList() {
    for (GtkPrinter* printer : printers_)
      g_object_unref(printer);
  }

  // Can return nullptr if there's no default printer. E.g. Printer on a laptop
  // is "home_printer", but the laptop is at work.
  GtkPrinter* default_printer() { return default_printer_; }

  // Can return nullptr if the printer cannot be found due to:
  // - Printer list out of sync with printer dialog UI.
  // - Querying for non-existant printers like 'Print to PDF'.
  GtkPrinter* GetPrinterWithName(const std::string& name) {
    if (name.empty())
      return nullptr;

    for (GtkPrinter* printer : printers_) {
      if (gtk_printer_get_name(printer) == name)
        return printer;
    }

    return nullptr;
  }

 private:
  // Callback function used by gtk_enumerate_printers() to get all printer.
  static gboolean SetPrinter(GtkPrinter* printer, gpointer data) {
    GtkPrinterList* printer_list = reinterpret_cast<GtkPrinterList*>(data);
    if (gtk_printer_is_default(printer))
      printer_list->default_printer_ = printer;

    g_object_ref(printer);
    printer_list->printers_.push_back(printer);

    return FALSE;
  }

  std::vector<GtkPrinter*> printers_;
  raw_ptr<GtkPrinter> default_printer_ = nullptr;
};

}  // namespace

// static
printing::PrintDialogLinuxInterface* PrintDialogGtk::CreatePrintDialog(
    PrintingContextLinux* context) {
  return new PrintDialogGtk(context);
}

PrintDialogGtk::PrintDialogGtk(PrintingContextLinux* context)
    : base::RefCountedDeleteOnSequence<PrintDialogGtk>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      context_(context) {
  // Paired with the ReleaseDialog() call.
  AddRef();
}

PrintDialogGtk::~PrintDialogGtk() {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  if (dialog_) {
    aura::Window* parent = gtk::GetAuraTransientParent(dialog_);
    if (parent) {
      parent->RemoveObserver(this);
      gtk::ClearAuraTransientParent(dialog_, parent);
    }
    gtk::GtkWindowDestroy(dialog_);
    dialog_ = nullptr;
  }
  if (gtk_settings_) {
    g_object_unref(gtk_settings_);
    gtk_settings_ = nullptr;
  }
  if (page_setup_) {
    g_object_unref(page_setup_);
    page_setup_ = nullptr;
  }
  if (printer_) {
    g_object_unref(printer_);
    printer_ = nullptr;
  }
}

void PrintDialogGtk::UseDefaultSettings() {
  DCHECK(!page_setup_);
  DCHECK(!printer_);

  // |gtk_settings_| is a new copy.
  gtk_settings_ = gtk_print_settings_copy(GetLastUsedSettings().settings());
  page_setup_ = gtk_page_setup_new();

  InitPrintSettings(std::make_unique<PrintSettings>());
}

void PrintDialogGtk::UpdateSettings(
    std::unique_ptr<printing::PrintSettings> settings) {
  if (!gtk_settings_)
    gtk_settings_ = gtk_print_settings_copy(GetLastUsedSettings().settings());

  auto printer_list = std::make_unique<GtkPrinterList>();
  printer_ = printer_list->GetPrinterWithName(
      base::UTF16ToUTF8(settings->device_name()));
  if (printer_) {
    g_object_ref(printer_);
    gtk_print_settings_set_printer(gtk_settings_,
                                   gtk_printer_get_name(printer_));
    if (!page_setup_) {
      page_setup_ = gtk_printer_get_default_page_size(printer_);
    }
  }

  gtk_print_settings_set_n_copies(gtk_settings_, settings->copies());
  gtk_print_settings_set_collate(gtk_settings_, settings->collate());
  if (settings->dpi_horizontal() > 0 && settings->dpi_vertical() > 0) {
    gtk_print_settings_set_resolution_xy(
        gtk_settings_, settings->dpi_horizontal(), settings->dpi_vertical());
#if BUILDFLAG(USE_CUPS)
    std::string dpi = base::NumberToString(settings->dpi_horizontal());
    if (settings->dpi_horizontal() != settings->dpi_vertical())
      dpi += "x" + base::NumberToString(settings->dpi_vertical());
    dpi += "dpi";

    // The resolution attribute (case-insensitive) has decent coverage
    // in the CUPS PPD API (Resolution, SetResolution, JCLResolution,
    // CNRes_PGP). See
    // https://chromium.googlesource.com/chromiumos/third_party/cups/+/49a182a4c42d/cups/mark.c#266
    // for more information.
    //
    // Many PPDs use pdftopdf directly to generate the print data and pdftopdf
    // uses the CUPS PPD API internally to handle resolution selection.
    //
    // Many third-party filters such as the Brother print filter that
    // do not use the CUPS PPD API are case sensitive and tend to support
    // the Resolution PPD attribute. For this reason "cups-Resolution"
    // makes the most sense here.
    //
    // TODO(crbug.com/1119956): Since PrintBackendCUPS parses the PPD file in
    // Chromium, it should be possible to store the resolution attribute name
    // as well as a map from the gfx::Size resolution to the std::string
    // serialized value (in case a non-standard value such as 500x500dpi is
    // present) in the PrinterCapsAndDefaults object. This object then needs to
    // be passed over here (there are a couple ways this can be done) where it
    // can be used to lookup the CUPS PPD resolution name and serialized DPI
    // value to use. The main benefit of the approach would be full support
    // for the HPPrintQuality and LXResolution PPD attributes which some PPD
    // files use.
    gtk_print_settings_set(gtk_settings_, "cups-Resolution", dpi.c_str());
#endif
  }

#if BUILDFLAG(USE_CUPS)
  // Set advanced settings first so they can be overridden by user applied
  // settings.
  static constexpr char kSettingNamePrefix[] = "cups-";
  for (const auto& pair : settings->advanced_settings()) {
    if (!pair.second.is_string())
      continue;
    const std::string setting_name = kSettingNamePrefix + pair.first;
    gtk_print_settings_set(gtk_settings_, setting_name.c_str(),
                           pair.second.GetString().c_str());
  }

  std::string color_value;
  std::string color_setting_name;
  printing::GetColorModelForModel(settings->color(), &color_setting_name,
                                  &color_value);
  color_setting_name.insert(0, kSettingNamePrefix);
  gtk_print_settings_set(gtk_settings_, color_setting_name.c_str(),
                         color_value.c_str());

  if (settings->duplex_mode() !=
      printing::mojom::DuplexMode::kUnknownDuplexMode) {
    const char* cups_duplex_mode = nullptr;
    switch (settings->duplex_mode()) {
      case printing::mojom::DuplexMode::kLongEdge:
        cups_duplex_mode = kDuplexNoTumble;
        break;
      case printing::mojom::DuplexMode::kShortEdge:
        cups_duplex_mode = kDuplexTumble;
        break;
      case printing::mojom::DuplexMode::kSimplex:
        cups_duplex_mode = kDuplexNone;
        break;
      default:  // kUnknownDuplexMode
        NOTREACHED();
        break;
    }
    gtk_print_settings_set(gtk_settings_, kCUPSDuplex, cups_duplex_mode);
  }
#endif

  if (!page_setup_)
    page_setup_ = gtk_page_setup_new();

  if (page_setup_ && !settings->requested_media().IsDefault()) {
    const PrintSettings::RequestedMedia& requested_media =
        settings->requested_media();
    GtkPaperSize* gtk_current_paper_size =
        gtk_page_setup_get_paper_size(page_setup_);
    if (!PaperSizeMatch(gtk_current_paper_size, requested_media,
                        true /*fuzzy_match*/)) {
      GList* gtk_paper_sizes =
          gtk_paper_size_get_paper_sizes(false /*include_custom*/);
      if (gtk_paper_sizes) {
        GtkPaperSize* matching_gtk_paper_size =
            FindPaperSizeMatch(gtk_paper_sizes, requested_media);
        if (matching_gtk_paper_size) {
          VLOG(1) << "Using listed paper size";
          gtk_page_setup_set_paper_size(page_setup_, matching_gtk_paper_size);
        } else {
          VLOG(1) << "Using custom paper size";
          GtkPaperSize* custom_size = gtk_paper_size_new_custom(
              requested_media.vendor_id.c_str(),
              requested_media.vendor_id.c_str(),
              requested_media.size_microns.width() / kMicronsInMm,
              requested_media.size_microns.height() / kMicronsInMm,
              GTK_UNIT_MM);
          gtk_page_setup_set_paper_size(page_setup_, custom_size);
          gtk_paper_size_free(custom_size);
        }
        g_list_free_full(gtk_paper_sizes,
                         reinterpret_cast<GDestroyNotify>(gtk_paper_size_free));
      }
    } else {
      VLOG(1) << "Using default paper size";
    }
  }

  gtk_print_settings_set_orientation(
      gtk_settings_, settings->landscape() ? GTK_PAGE_ORIENTATION_LANDSCAPE
                                           : GTK_PAGE_ORIENTATION_PORTRAIT);

  InitPrintSettings(std::move(settings));
}

void PrintDialogGtk::ShowDialog(
    gfx::NativeView parent_view,
    bool has_selection,
    PrintingContextLinux::PrintSettingsCallback callback) {
  callback_ = std::move(callback);
  DCHECK(callback_);

  dialog_ = gtk_print_unix_dialog_new(nullptr, nullptr);
  gtk::SetGtkTransientForAura(dialog_, parent_view);
  if (parent_view)
    parent_view->AddObserver(this);
  if (gtk::GtkCheckVersion(4)) {
    gtk_window_set_hide_on_close(GTK_WINDOW(dialog_), true);
  } else {
    g_signal_connect(dialog_, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), nullptr);
  }

  // Handle the case when the existing |gtk_settings_| has "selection" selected
  // as the page range, but |has_selection| is false.
  if (!has_selection) {
    GtkPrintPages range = gtk_print_settings_get_print_pages(gtk_settings_);
    if (range == GTK_PRINT_PAGES_SELECTION)
      gtk_print_settings_set_print_pages(gtk_settings_, GTK_PRINT_PAGES_ALL);
  }

  // Set modal so user cannot focus the same tab and press print again.
  gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);

  // Since we only generate PDF, only show printers that support PDF.
  // TODO(thestig) Add more capabilities to support?
  GtkPrintCapabilities cap = static_cast<GtkPrintCapabilities>(
      GTK_PRINT_CAPABILITY_GENERATE_PDF | GTK_PRINT_CAPABILITY_PAGE_SET |
      GTK_PRINT_CAPABILITY_COPIES | GTK_PRINT_CAPABILITY_COLLATE |
      GTK_PRINT_CAPABILITY_REVERSE);
  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(dialog_),
                                                cap);
  gtk_print_unix_dialog_set_embed_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_),
                                             TRUE);
  gtk_print_unix_dialog_set_support_selection(GTK_PRINT_UNIX_DIALOG(dialog_),
                                              TRUE);
  gtk_print_unix_dialog_set_has_selection(GTK_PRINT_UNIX_DIALOG(dialog_),
                                          has_selection);
  gtk_print_unix_dialog_set_settings(GTK_PRINT_UNIX_DIALOG(dialog_),
                                     gtk_settings_);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_widget_show(dialog_);

  gtk::GtkUi::GetPlatform()->ShowGtkWindow(GTK_WINDOW(dialog_));
}

void PrintDialogGtk::PrintDocument(const printing::MetafilePlayer& metafile,
                                   const std::u16string& document_name) {
#if DCHECK_IS_ON()
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  const bool kOopPrinting =
      printing::features::kEnableOopPrintDriversJobPrint.Get();
#else
  const bool kOopPrinting = false;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  // For in-browser printing, this runs on the print worker thread, so it does
  // not block the UI thread.  For OOP it runs on the service document task
  // runner.
  DCHECK_EQ(owning_task_runner()->RunsTasksInCurrentSequence(), kOopPrinting);
#endif  // DCHECK_IS_ON()

  // The document printing tasks can outlive the PrintingContext that created
  // this dialog.
  AddRef();

  bool success = base::CreateTemporaryFile(&path_to_pdf_);

  if (success) {
    base::File file;
    file.Initialize(path_to_pdf_,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    success = metafile.SaveTo(&file);
    file.Close();
    if (!success)
      base::DeleteFile(path_to_pdf_);
  }

  if (!success) {
    LOG(ERROR) << "Saving metafile failed";
    // Matches AddRef() above.
    Release();
    return;
  }

  // No errors, continue printing.
  owning_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PrintDialogGtk::SendDocumentToPrinter, this,
                                document_name));
}

void PrintDialogGtk::ReleaseDialog() {
  context_ = nullptr;
  Release();
}

void PrintDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  int num_matched_handlers = g_signal_handlers_disconnect_by_func(
      dialog_, reinterpret_cast<gpointer>(&OnResponseThunk), this);
  CHECK_EQ(1, num_matched_handlers);

  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      if (!context_) {
        std::move(callback_).Run(printing::mojom::ResultCode::kCanceled);
        return;
      }

      if (gtk_settings_)
        g_object_unref(gtk_settings_);
      gtk_settings_ =
          gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog_));

      if (printer_)
        g_object_unref(printer_);
      printer_ = gtk_print_unix_dialog_get_selected_printer(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(printer_);

      if (page_setup_)
        g_object_unref(page_setup_);
      page_setup_ =
          gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(page_setup_);

      // Handle page ranges.
      PageRanges ranges_vector;
      gint num_ranges;
      bool print_selection_only = false;
      switch (gtk_print_settings_get_print_pages(gtk_settings_)) {
        case GTK_PRINT_PAGES_RANGES: {
          GtkPageRange* gtk_range =
              gtk_print_settings_get_page_ranges(gtk_settings_, &num_ranges);
          if (gtk_range) {
            for (int i = 0; i < num_ranges; ++i) {
              printing::PageRange range;
              range.from = gtk_range[i].start;
              range.to = gtk_range[i].end;
              ranges_vector.push_back(range);
            }
            g_free(gtk_range);
          }
          break;
        }
        case GTK_PRINT_PAGES_SELECTION:
          print_selection_only = true;
          break;
        case GTK_PRINT_PAGES_ALL:
          // Leave |ranges_vector| empty to indicate print all pages.
          break;
        case GTK_PRINT_PAGES_CURRENT:
        default:
          NOTREACHED();
          break;
      }

      auto settings = std::make_unique<PrintSettings>();
      settings->set_is_modifiable(context_->settings().is_modifiable());
      settings->set_ranges(ranges_vector);
      settings->set_selection_only(print_selection_only);
      InitPrintSettingsGtk(gtk_settings_, page_setup_, settings.get());
      context_->InitWithSettings(std::move(settings));
      std::move(callback_).Run(printing::mojom::ResultCode::kSuccess);
      return;
    }
    case GTK_RESPONSE_DELETE_EVENT:  // Fall through.
    case GTK_RESPONSE_CANCEL: {
      std::move(callback_).Run(printing::mojom::ResultCode::kCanceled);
      return;
    }
    case GTK_RESPONSE_APPLY:
    default: {
      NOTREACHED();
    }
  }
}

static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                gpointer user_data,
                                const GError* error) {
  static_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job, error);
}
void PrintDialogGtk::SendDocumentToPrinter(
    const std::u16string& document_name) {
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());

  // If |printer_| is nullptr then somehow the GTK printer list changed out
  // under us. In which case, just bail out.
  if (!printer_) {
    // Matches AddRef() in PrintDocument();
    Release();
    return;
  }

  // Save the settings for next time.
  GetLastUsedSettings().SetLastUsedSettings(gtk_settings_);

  GtkPrintJob* print_job =
      gtk_print_job_new(base::UTF16ToUTF8(document_name).c_str(), printer_,
                        gtk_settings_, page_setup_);
  gtk_print_job_set_source_file(print_job, path_to_pdf_.value().c_str(),
                                nullptr);
  gtk_print_job_send(print_job, OnJobCompletedThunk, this, nullptr);
}

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* print_job,
                                    const GError* error) {
  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;
  if (print_job)
    g_object_unref(print_job);

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::GetDeleteFileCallback(path_to_pdf_));
  // Printing finished. Matches AddRef() in PrintDocument();
  Release();
}

void PrintDialogGtk::InitPrintSettings(
    std::unique_ptr<PrintSettings> settings) {
  if (!context_)
    return;

  InitPrintSettingsGtk(gtk_settings_, page_setup_, settings.get());
  context_->InitWithSettings(std::move(settings));
}

void PrintDialogGtk::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(gtk::GetAuraTransientParent(dialog_), window);

  gtk::ClearAuraTransientParent(dialog_, window);
  window->RemoveObserver(this);
  if (callback_)
    std::move(callback_).Run(printing::mojom::ResultCode::kCanceled);
}
