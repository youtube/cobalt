// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/ui_navigation/nav_item.h"

namespace cobalt {
namespace ui_navigation {

NativeCallbacks NavItem::s_callbacks_ = {
  &NavItem::OnBlur,
  &NavItem::OnFocus,
  &NavItem::OnScroll,
};

NavItem::NavItem(NativeItemType type,
                 const base::Closure& onblur_callback,
                 const base::Closure& onfocus_callback,
                 const base::Closure& onscroll_callback)
    : onblur_callback_(onblur_callback),
      onfocus_callback_(onfocus_callback),
      onscroll_callback_(onscroll_callback),
      nav_item_type_(type),
      nav_item_(GetInterface().create_item(type, &s_callbacks_, this)) {}

NavItem::~NavItem() {
  GetInterface().set_item_enabled(nav_item_, false);
  GetInterface().destroy_item(nav_item_);
}

// static
void NavItem::OnBlur(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  if (!this_ptr->onblur_callback_.is_null()) {
    this_ptr->onblur_callback_.Run();
  }
}

// static
void NavItem::OnFocus(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  if (!this_ptr->onfocus_callback_.is_null()) {
    this_ptr->onfocus_callback_.Run();
  }
}

// static
void NavItem::OnScroll(NativeItem item, void* callback_context) {
  NavItem* this_ptr = static_cast<NavItem*>(callback_context);
  if (!this_ptr->onscroll_callback_.is_null()) {
    this_ptr->onscroll_callback_.Run();
  }
}

}  // namespace ui_navigation
}  // namespace cobalt
