// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "url/gurl.h"

@interface CrPasteboardItemWrapper : NSObject <NSPasteboardWriting>
- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem;
@end

@implementation CrPasteboardItemWrapper {
  base::scoped_nsobject<NSPasteboardItem> _pasteboardItem;
}

- (instancetype)initWithPasteboardItem:(NSPasteboardItem*)pasteboardItem {
  if ((self = [super init])) {
    _pasteboardItem.reset([pasteboardItem retain]);
  }

  return self;
}

- (NSArray<NSString*>*)writableTypesForPasteboard:(NSPasteboard*)pasteboard {
  // If the NSPasteboardItem hasn't been added to an NSPasteboard, then the
  // -[NSPasteboardItem writableTypesForPasteboard:] will return -types. But if
  // it has been added to a pasteboard, it will return nil. This pasteboard item
  // was added implicitly by adding flavors to the owned pasteboard of
  // OwningProvider, so call -types to actually get data.
  //
  // Merge in the ui::kUTTypeChromiumInitiatedDrag type, so that all of Chromium
  // is marked to receive the drags. TODO(avi): Wire up MacViews so that
  // BridgedContentView properly registers the result of View::GetDropFormats()
  // rather than OSExchangeDataProviderMac::SupportedPasteboardTypes().
  return [[_pasteboardItem types]
      arrayByAddingObject:ui::kUTTypeChromiumInitiatedDrag];
}

- (NSPasteboardWritingOptions)writingOptionsForType:(NSString*)type
                                         pasteboard:(NSPasteboard*)pasteboard {
  // It is critical to return 0 here. If any flavors are promised, then when the
  // app quits, AppKit will call in the promises, and the backing pasteboard
  // will likely be long-deallocated. Yes, AppKit will call in promises for
  // *all* promised flavors on *all* pasteboards, not just those pasteboards
  // used for copy/paste.
  return 0;
}

- (id)pasteboardPropertyListForType:(NSString*)type {
  if ([type isEqual:ui::kUTTypeChromiumInitiatedDrag])
    return [NSData data];

  // Like above, an NSPasteboardItem added to a pasteboard will return nil from
  // -pasteboardPropertyListForType:, so call -dataForType: instead.
  return [_pasteboardItem dataForType:type];
}

@end

namespace ui {

namespace {

class OwningProvider : public OSExchangeDataProviderMac {
 public:
  OwningProvider() : owned_pasteboard_(new UniquePasteboard) {}
  OwningProvider(const OwningProvider& provider) = default;

  std::unique_ptr<OSExchangeDataProvider> Clone() const override {
    return std::make_unique<OwningProvider>(*this);
  }

  NSPasteboard* GetPasteboard() const override {
    return owned_pasteboard_->get();
  }

 private:
  scoped_refptr<UniquePasteboard> owned_pasteboard_;
};

class WrappingProvider : public OSExchangeDataProviderMac {
 public:
  explicit WrappingProvider(NSPasteboard* pasteboard)
      : wrapped_pasteboard_([pasteboard retain]) {}
  WrappingProvider(const WrappingProvider& provider) = default;

  std::unique_ptr<OSExchangeDataProvider> Clone() const override {
    return std::make_unique<WrappingProvider>(*this);
  }

  NSPasteboard* GetPasteboard() const override { return wrapped_pasteboard_; }

 private:
  base::scoped_nsobject<NSPasteboard> wrapped_pasteboard_;
};

}  // namespace

OSExchangeDataProviderMac::OSExchangeDataProviderMac() = default;
OSExchangeDataProviderMac::OSExchangeDataProviderMac(
    const OSExchangeDataProviderMac&) = default;
OSExchangeDataProviderMac& OSExchangeDataProviderMac::operator=(
    const OSExchangeDataProviderMac&) = default;

OSExchangeDataProviderMac::~OSExchangeDataProviderMac() = default;

// static
std::unique_ptr<OSExchangeDataProviderMac>
OSExchangeDataProviderMac::CreateProvider() {
  return std::make_unique<OwningProvider>();
}

// static
std::unique_ptr<OSExchangeDataProviderMac>
OSExchangeDataProviderMac::CreateProviderWrappingPasteboard(
    NSPasteboard* pasteboard) {
  return std::make_unique<WrappingProvider>(pasteboard);
}

void OSExchangeDataProviderMac::MarkOriginatedFromRenderer() {
  [GetPasteboard() setData:[NSData data]
                   forType:kUTTypeChromiumRendererInitiatedDrag];
}

bool OSExchangeDataProviderMac::DidOriginateFromRenderer() const {
  return [GetPasteboard().types
      containsObject:kUTTypeChromiumRendererInitiatedDrag];
}

void OSExchangeDataProviderMac::MarkAsFromPrivileged() {
  [GetPasteboard() setData:[NSData data]
                   forType:kUTTypeChromiumPrivilegedInitiatedDrag];
}

bool OSExchangeDataProviderMac::IsFromPrivileged() const {
  return [GetPasteboard().types
      containsObject:kUTTypeChromiumPrivilegedInitiatedDrag];
}

void OSExchangeDataProviderMac::SetString(const std::u16string& string) {
  [GetPasteboard() setString:base::SysUTF16ToNSString(string)
                     forType:NSPasteboardTypeString];
}

void OSExchangeDataProviderMac::SetURL(const GURL& url,
                                       const std::u16string& title) {
  NSArray<NSPasteboardItem*>* items = clipboard_util::PasteboardItemsFromUrls(
      @[ base::SysUTF8ToNSString(url.spec()) ],
      @[ base::SysUTF16ToNSString(title) ]);
  clipboard_util::AddDataToPasteboard(GetPasteboard(), items.firstObject);
}

void OSExchangeDataProviderMac::SetFilename(const base::FilePath& path) {
  std::vector<FileInfo> filenames(1, FileInfo(path, base::FilePath()));
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void OSExchangeDataProviderMac::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  clipboard_util::WriteFilesToPasteboard(GetPasteboard(), filenames);
}

void OSExchangeDataProviderMac::SetPickledData(
    const ClipboardFormatType& format,
    const base::Pickle& data) {
  NSData* ns_data = [NSData dataWithBytes:data.data() length:data.size()];
  [GetPasteboard() setData:ns_data forType:format.ToNSString()];
}

bool OSExchangeDataProviderMac::GetString(std::u16string* data) const {
  DCHECK(data);
  NSString* item = [GetPasteboard() stringForType:NSPasteboardTypeString];
  if (item) {
    *data = base::SysNSStringToUTF16(item);
    return true;
  }

  // There was no NSString, check for an NSURL.
  GURL url;
  std::u16string title;
  bool result = GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES,
                               &url, &title);
  if (result)
    *data = base::UTF8ToUTF16(url.spec());

  return result;
}

bool OSExchangeDataProviderMac::GetURLAndTitle(FilenameToURLPolicy policy,
                                               GURL* url,
                                               std::u16string* title) const {
  DCHECK(url);
  DCHECK(title);

  NSArray<NSString*>* urls;
  NSArray<NSString*>* titles;
  if (clipboard_util::URLsAndTitlesFromPasteboard(
          GetPasteboard(), /*include_files=*/false, &urls, &titles)) {
    *url = GURL(base::SysNSStringToUTF8(urls.firstObject));
    *title = base::SysNSStringToUTF16(titles.firstObject);
    return true;
  }

  // If there are no URLs, try to convert a filename to a URL if the policy
  // allows it. The title remains blank.
  //
  // This could be done in the call to `URLsAndTitlesFromPasteboard` above if
  // `true` were passed in for the `include_files` parameter, but that function
  // strips the trailing slashes off of paths and always returns the last path
  // element as the title whereas no path conversion nor title is wanted.
  //
  // TODO(avi): What is going on here? This comment and code was written for the
  // old pasteboard code; is this still true with the new pasteboard code? What
  // uses this, and why does it care about titles or path conversion?
  base::FilePath path;
  if (policy != FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES &&
      GetFilename(&path)) {
    *url = net::FilePathToFileURL(path);
    return true;
  }

  return false;
}

bool OSExchangeDataProviderMac::GetFilename(base::FilePath* path) const {
  std::vector<FileInfo> files =
      clipboard_util::FilesFromPasteboard(GetPasteboard());
  if (files.empty()) {
    return false;
  }

  *path = files[0].path;
  return true;
}

bool OSExchangeDataProviderMac::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  std::vector<FileInfo> files =
      clipboard_util::FilesFromPasteboard(GetPasteboard());
  bool result = !files.empty();
  base::ranges::move(files, std::back_inserter(*filenames));
  return result;
}

bool OSExchangeDataProviderMac::GetPickledData(
    const ClipboardFormatType& format,
    base::Pickle* data) const {
  DCHECK(data);
  NSData* ns_data = [GetPasteboard() dataForType:format.ToNSString()];
  if (!ns_data)
    return false;

  *data =
      base::Pickle(static_cast<const char*>([ns_data bytes]), [ns_data length]);
  return true;
}

bool OSExchangeDataProviderMac::HasString() const {
  std::u16string string;
  return GetString(&string);
}

bool OSExchangeDataProviderMac::HasURL(FilenameToURLPolicy policy) const {
  GURL url;
  std::u16string title;
  return GetURLAndTitle(policy, &url, &title);
}

bool OSExchangeDataProviderMac::HasFile() const {
  return [GetPasteboard().types containsObject:NSPasteboardTypeFileURL];
}

bool OSExchangeDataProviderMac::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return [GetPasteboard().types containsObject:format.ToNSString()];
}

void OSExchangeDataProviderMac::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  NOTIMPLEMENTED();
}

bool OSExchangeDataProviderMac::GetFileContents(
    base::FilePath* filename,
    std::string* file_contents) const {
  NOTIMPLEMENTED();
  return false;
}

bool OSExchangeDataProviderMac::HasFileContents() const {
  NOTIMPLEMENTED();
  return false;
}

void OSExchangeDataProviderMac::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  cursor_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderMac::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderMac::GetDragImageOffset() const {
  return cursor_offset_;
}

NSArray<NSDraggingItem*>* OSExchangeDataProviderMac::GetDraggingItems() const {
  // What's going on here is that initiating a drag (-[NSView
  // beginDraggingSessionWithItems...]) requires a dragging item. Even though
  // pasteboard items are NSPasteboardWriters, they are locked to their
  // pasteboard and cannot be used to initiate a drag with another pasteboard
  // (hello https://crbug.com/928684). Therefore, wrap them.

  NSArray<NSPasteboardItem*>* pasteboard_items =
      GetPasteboard().pasteboardItems;
  if (!pasteboard_items) {
    return nil;
  }

  NSMutableArray<NSDraggingItem*>* drag_items = [NSMutableArray array];
  for (NSPasteboardItem* item in pasteboard_items) {
    CrPasteboardItemWrapper* wrapper = [[[CrPasteboardItemWrapper alloc]
        initWithPasteboardItem:item] autorelease];
    NSDraggingItem* drag_item =
        [[[NSDraggingItem alloc] initWithPasteboardWriter:wrapper] autorelease];

    [drag_items addObject:drag_item];
  }

  return drag_items;
}

// static
NSArray* OSExchangeDataProviderMac::SupportedPasteboardTypes() {
  return @[
    kUTTypeChromiumInitiatedDrag, kUTTypeChromiumPrivilegedInitiatedDrag,
    kUTTypeChromiumRendererInitiatedDrag, kUTTypeChromiumWebCustomData,
    kUTTypeWebKitWebURLsWithTitles, NSPasteboardTypeFileURL,
    NSPasteboardTypeHTML, NSPasteboardTypeRTF, NSPasteboardTypeString,
    NSPasteboardTypeURL
  ];
}

void OSExchangeDataProviderMac::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {}

DataTransferEndpoint* OSExchangeDataProviderMac::GetSource() const {
  return nullptr;
}

}  // namespace ui
