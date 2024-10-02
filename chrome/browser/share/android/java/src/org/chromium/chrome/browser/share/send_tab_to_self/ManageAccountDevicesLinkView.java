// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** View containing the sharing account's avatar, email and a link to manage its target devices. */
public class ManageAccountDevicesLinkView extends LinearLayout {
    private static final int ACCOUNT_AVATAR_SIZE_DP = 24;

    private final boolean mShowLink;

    public ManageAccountDevicesLinkView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray attributes = context.getTheme().obtainStyledAttributes(
                attrs, R.styleable.ManageAccountDevicesLinkView, 0, 0);
        try {
            mShowLink =
                    attributes.getBoolean(R.styleable.ManageAccountDevicesLinkView_showLink, false);
        } finally {
            attributes.recycle();
        }
        inflateIfVisible();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        inflateIfVisible();
    }

    // TODO(crbug.com/1219434): For now the account information is only filled once the view becomes
    // visible, so it can still be declared in the XML if there is no account. After launch, fill
    // the data immediately.
    private void inflateIfVisible() {
        if (getVisibility() != View.VISIBLE) {
            return;
        }

        // The view was already inflated, nothing else to do.
        if (getChildCount() > 0) {
            return;
        }

        LayoutInflater.from(getContext())
                .inflate(R.layout.send_tab_to_self_manage_devices_link, this);

        AccountInfo account = getSharingAccountInfo();
        assert account != null;

        // The avatar can be null in tests.
        if (account.getAccountImage() != null) {
            RoundedCornerImageView avatarView = findViewById(R.id.account_avatar);
            int accountAvatarSizePx =
                    Math.round(ACCOUNT_AVATAR_SIZE_DP * getResources().getDisplayMetrics().density);
            avatarView.setImageBitmap(Bitmap.createScaledBitmap(
                    account.getAccountImage(), accountAvatarSizePx, accountAvatarSizePx, false));
            avatarView.setRoundedCorners(accountAvatarSizePx / 2, accountAvatarSizePx / 2,
                    accountAvatarSizePx / 2, accountAvatarSizePx / 2);
        }

        TextView linkView = findViewById(R.id.manage_devices_link);
        // If the feature is disabled, the email address is displayable by default.
        final boolean canHaveEmailAddressDisplayed =
                account.getAccountCapabilities().canHaveEmailAddressDisplayed() != Tribool.FALSE
                || !ChromeFeatureList.sHideNonDisplayableAccountEmail.isEnabled();
        final String accountFullNameOrEmail =
                canHaveEmailAddressDisplayed ? account.getEmail() : account.getFullName();
        if (mShowLink) {
            SpannableString linkText = SpanApplier.applySpans(
                    getResources().getString(
                            R.string.send_tab_to_self_manage_devices_link, accountFullNameOrEmail),
                    new SpanApplier.SpanInfo("<link>", "</link>",
                            new NoUnderlineClickableSpan(
                                    getContext(), this::openManageDevicesPageInNewTab)));
            linkView.setText(linkText);
            linkView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            linkView.setText(accountFullNameOrEmail);
        }
    }

    private void openManageDevicesPageInNewTab(View unused) {
        // The link is opened in a new tab to avoid exiting the current page, which the user
        // possibly wants to share (maybe they just clicked "Manage devices" by mistake).
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(UrlConstants.GOOGLE_ACCOUNT_DEVICE_ACTIVITY_URL))
                        .setClass(getContext(), ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        getContext().startActivity(intent);
    }

    private static AccountInfo getSharingAccountInfo() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return identityManager.findExtendedAccountInfoByEmailAddress(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN).getEmail());
    }
}
