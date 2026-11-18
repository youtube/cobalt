// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"

namespace bookmarks_api {

namespace {

template <typename T>
base::expected<void, mojo_base::mojom::ErrorPtr> CheckNotNull(
    const T& ptr,
    std::string_view name) {
  if (!ptr) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     base::StrCat({name, " cannot be null"})));
  }
  return base::ok();
}

mojo_base::mojom::ErrorPtr MakeError(mojo_base::mojom::Code code,
                                     std::string_view message) {
  return mojo_base::mojom::Error::New(code, std::string(message));
}

}  // namespace

BookmarksServiceImpl::BookmarksServiceImpl(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model), finder_(bookmark_model) {
  CHECK(bookmark_model_);
  CHECK(bookmark_model_->loaded());
}

BookmarksServiceImpl::~BookmarksServiceImpl() = default;

void BookmarksServiceImpl::Accept(
    mojo::PendingReceiver<mojom::BookmarksService> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::BookmarksService::GetBookmarksResult
BookmarksServiceImpl::GetBookmarks() {
  return ConvertNode(bookmark_model_->root_node());
}

mojom::BookmarksService::GetBookmarkResult BookmarksServiceImpl::GetBookmark(
    const base::Uuid& id) {
  ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* node,
                   finder_.FindNodeByUuid(id), &MakeError,
                   mojo_base::mojom::Code::kNotFound, "Bookmark not found");
  return ConvertNode(node);
}

mojom::BookmarkNodePtr BookmarksServiceImpl::ConvertNode(
    const bookmarks::BookmarkNode* node) {
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL: {
      auto url_node = mojom::Url::New();
      url_node->id = node->uuid();
      url_node->title = base::UTF16ToUTF8(node->GetTitle());
      url_node->url = node->url();
      return mojom::BookmarkNode::NewUrl(std::move(url_node));
    }
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE: {
      auto folder_node = mojom::Folder::New();
      folder_node->id = node->uuid();
      folder_node->title = base::UTF16ToUTF8(node->GetTitle());
      for (const auto& child : node->children()) {
        folder_node->children.push_back(ConvertNode(child.get()));
      }
      return mojom::BookmarkNode::NewFolder(std::move(folder_node));
    }
  }
}

mojom::BookmarksService::CreateBookmarkNodeResult
BookmarksServiceImpl::CreateBookmarkNode(const base::Uuid& parent_id,
                                         std::optional<int32_t> index,
                                         mojom::BookmarkNodePtr node) {
  RETURN_IF_ERROR(CheckNotNull(node, "Node"));

  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* parent, finder_.FindNodeByUuid(parent_id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Parent folder not found");

  if (!parent->is_folder()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Parent node is not a folder"));
  }

  size_t target_index;
  if (index.has_value()) {
    int32_t val = index.value();
    if (val < 0) {
      return base::unexpected(
          mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                       "Index cannot be negative"));
    }
    target_index = static_cast<size_t>(val);
    if (target_index > parent->children().size()) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "Index out of range"));
    }
  } else {
    target_index = parent->children().size();
  }

  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      const auto& url_data = node->get_url();
      GURL gurl = url_data->url;
      if (!gurl.is_valid()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "Invalid URL"));
      }
      auto* new_node = bookmark_model_->AddNewURL(
          parent, target_index, base::UTF8ToUTF16(url_data->title), gurl);
      if (!new_node) {
        return base::unexpected(
            mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInternal,
                                         "Failed to create bookmark node"));
      }
      return ConvertNode(new_node);
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      const auto& folder_data = node->get_folder();
      auto* new_node = bookmark_model_->AddFolder(
          parent, target_index, base::UTF8ToUTF16(folder_data->title));
      if (!new_node) {
        return base::unexpected(
            mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInternal,
                                         "Failed to create bookmark node"));
      }
      return ConvertNode(new_node);
    }
  }
}

mojom::BookmarksService::UpdateBookmarkNodeResult
BookmarksServiceImpl::UpdateBookmarkNode(mojom::BookmarkNodePtr node) {
  RETURN_IF_ERROR(CheckNotNull(node, "Node"));

  base::Uuid id;
  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      const auto& url_data = node->get_url();
      if (!url_data->id.has_value()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "bookmark id is required"));
      }
      id = url_data->id.value();
      break;
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      const auto& folder_data = node->get_folder();
      if (!folder_data->id.has_value()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "folder id is required"));
      }
      id = folder_data->id.value();
      break;
    }
  }

  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* model_node, finder_.FindNodeByUuid(id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Bookmark node not found");

  if (bookmark_model_->is_permanent_node(model_node)) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Cannot update permanent node"));
  }

  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      if (model_node->is_folder()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Cannot update folder with URL data"));
      }
      const auto& url_data = node->get_url();
      GURL gurl = url_data->url;
      if (!gurl.is_valid()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "Invalid URL"));
      }
      bookmark_model_->SetTitle(model_node, base::UTF8ToUTF16(url_data->title),
                                bookmarks::metrics::BookmarkEditSource::kUser);
      bookmark_model_->SetURL(model_node, gurl,
                              bookmarks::metrics::BookmarkEditSource::kUser);
      ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* updated_node,
                       finder_.FindNodeByUuid(id), &MakeError,
                       mojo_base::mojom::Code::kInternal,
                       "Failed to refetch updated node");
      return ConvertNode(updated_node);
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      if (model_node->is_url()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Cannot update URL node with folder data"));
      }
      const auto& folder_data = node->get_folder();
      bookmark_model_->SetTitle(model_node,
                                base::UTF8ToUTF16(folder_data->title),
                                bookmarks::metrics::BookmarkEditSource::kUser);
      ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* updated_node,
                       finder_.FindNodeByUuid(id), &MakeError,
                       mojo_base::mojom::Code::kInternal,
                       "Failed to refetch updated node");
      return ConvertNode(updated_node);
    }
  }
}

mojom::BookmarksService::DeleteBookmarkNodeResult
BookmarksServiceImpl::DeleteBookmarkNode(const base::Uuid& id) {
  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* node, finder_.FindNodeByUuid(id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Bookmark node not found");

  if (bookmark_model_->is_permanent_node(node)) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Cannot delete permanent node"));
  }

  bookmark_model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                          FROM_HERE);

  return std::monostate();
}

}  // namespace bookmarks_api
