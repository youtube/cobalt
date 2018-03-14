// Copyright 2017 Google Inc. All Rights Reserved.
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

  private final Params params;

  private static class Params {
    private int messageId;
    private int numButtons = 0;
    private int[] buttonIds = new int[MAX_BUTTONS];
    private int[] buttonLabelIds = new int[MAX_BUTTONS];
    private OnClickListener buttonClickListener;
    private OnDismissListener dismissListener;
  }

  public static class Builder {

    private Context context;
    private Params params = new Params();

    public Builder(Context context) {
      this.context = context;
    }

    public Builder setMessage(int messageId) {
      params.messageId = messageId;
      return this;
    }

    public Builder addButton(int id, int labelId) {
      if (params.numButtons >= MAX_BUTTONS) {
        throw new IllegalArgumentException("Too many buttons");
      }
      params.buttonIds[params.numButtons] = id;
      params.buttonLabelIds[params.numButtons] = labelId;
      params.numButtons++;
      return this;
    }

    public Builder setButtonClickListener(OnClickListener buttonClickListener) {
      params.buttonClickListener = buttonClickListener;
      return this;
    }

    public Builder setOnDismissListener(OnDismissListener dismissListener) {
      params.dismissListener = dismissListener;
      return this;
    }

    public ErrorDialog create() {
      return new ErrorDialog(context, params);
    }
  }

  private ErrorDialog(Context context, Params params) {
    super(context);
    this.params = params;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.coat_error_dialog);

    ImageView imageView = (ImageView) findViewById(R.id.image);
    Drawable drawable = getContext().getResources().getDrawable(
        R.drawable.lb_ic_sad_cloud, getContext().getTheme());
    imageView.setImageDrawable(drawable);

    TextView messageView = (TextView) findViewById(R.id.message);
    messageView.setText(params.messageId);

    Button button = (Button) findViewById(R.id.button_1);
    ViewGroup container = (ViewGroup) button.getParent();
    int buttonIndex = container.indexOfChild(button);

    for (int i = 0; i < params.numButtons; i++) {
      button = (Button) container.getChildAt(buttonIndex + i);
      button.setText(params.buttonLabelIds[i]);
      button.setVisibility(View.VISIBLE);

      final int buttonId = params.buttonIds[i];
      button.setOnClickListener(new View.OnClickListener() {
        @Override
        public void onClick(View v) {
          params.buttonClickListener.onClick(ErrorDialog.this, buttonId);
        }
      });
    }

    setOnDismissListener(params.dismissListener);
  }

}
