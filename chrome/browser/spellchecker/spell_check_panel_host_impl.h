// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_PANEL_HOST_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_PANEL_HOST_IMPL_H_

#include "base/functional/callback.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if !BUILDFLAG(HAS_SPELLCHECK_PANEL)
#error "Spellcheck panel should be enabled."
#endif

class SpellCheckPanelHostImpl : public spellcheck::mojom::SpellCheckPanelHost {
 public:
  SpellCheckPanelHostImpl();

  SpellCheckPanelHostImpl(const SpellCheckPanelHostImpl&) = delete;
  SpellCheckPanelHostImpl& operator=(const SpellCheckPanelHostImpl&) = delete;

  ~SpellCheckPanelHostImpl() override;

  static void Create(
      int render_process_id,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver);

  // Allows tests to override how |Create()| is implemented to bind a process
  // hosts's SpellCheckPanelHost receiver.
  using Binder = base::RepeatingCallback<void(
      int /* render_process_id */,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost>)>;
  static void OverrideBinderForTesting(Binder binder);

 private:
  // spellcheck::mojom::SpellCheckPanelHost:
  void ShowSpellingPanel(bool show) override;
  void UpdateSpellingPanelWithMisspelledWord(
      const std::u16string& word) override;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_PANEL_HOST_IMPL_H_
