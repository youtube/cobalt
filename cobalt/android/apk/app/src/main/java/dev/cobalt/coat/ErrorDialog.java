// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import android.app.Dialog;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * A fullscreen dialog to show an error, with up to 3 buttons. This has a look similar to the
 * Android TV leanback ErrorFragment. As a dialog, it creates its own window so it can be shown on
 * top of our NativeActivity, unlike a fragment.
 */
class ErrorDialog extends Dialog {

  public static final int MAX_BUTTONS = 3;

  private final Params mParams;

  private static class Params {
    private int mMessageId;
    private int mNumButtons = 0;
    private int[] mButtonIds = new int[MAX_BUTTONS];
    private int[] mButtonLabelIds = new int[MAX_BUTTONS];
    private OnClickListener mButtonClickListener;
    private OnDismissListener mDismissListener;
  }

  public static class Builder {

    private Context mContext;
    private Params mParams = new Params();

    public Builder(Context context) {
      this.mContext = context;
    }

    public Builder setMessage(int messageId) {
      mParams.mMessageId = messageId;
      return this;
    }

    public Builder addButton(int id, int labelId) {
      if (mParams.mNumButtons >= MAX_BUTTONS) {
        throw new IllegalArgumentException("Too many buttons");
      }
      mParams.mButtonIds[mParams.mNumButtons] = id;
      mParams.mButtonLabelIds[mParams.mNumButtons] = labelId;
      mParams.mNumButtons++;
      return this;
    }

    public Builder setButtonClickListener(OnClickListener buttonClickListener) {
      mParams.mButtonClickListener = buttonClickListener;
      return this;
    }

    public Builder setOnDismissListener(OnDismissListener dismissListener) {
      mParams.mDismissListener = dismissListener;
      return this;
    }

    public ErrorDialog create() {
      return new ErrorDialog(mContext, mParams);
    }
  }

  private ErrorDialog(Context context, Params params) {
    super(context);
    this.mParams = params;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.coat_error_dialog);

    ImageView imageView = (ImageView) findViewById(R.id.image);
    Drawable drawable =
        getContext()
            .getResources()
            .getDrawable(R.drawable.lb_ic_sad_cloud, getContext().getTheme());
    imageView.setImageDrawable(drawable);

    TextView messageView = (TextView) findViewById(R.id.message);
    messageView.setText(mParams.mMessageId);

    Button button = (Button) findViewById(R.id.button_1);
    ViewGroup container = (ViewGroup) button.getParent();
    int buttonIndex = container.indexOfChild(button);

    for (int i = 0; i < mParams.mNumButtons; i++) {
      button = (Button) container.getChildAt(buttonIndex + i);
      button.setText(mParams.mButtonLabelIds[i]);
      button.setVisibility(View.VISIBLE);

      final int buttonId = mParams.mButtonIds[i];
      button.setOnClickListener(
          new View.OnClickListener() {
            @Override
            public void onClick(View v) {
              mParams.mButtonClickListener.onClick(ErrorDialog.this, buttonId);
            }
          });
    }

    setOnDismissListener(mParams.mDismissListener);
  }
}
