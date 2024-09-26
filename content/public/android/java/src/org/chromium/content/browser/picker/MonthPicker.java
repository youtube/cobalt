// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.picker;

import android.annotation.SuppressLint;
import android.content.Context;

import org.chromium.content.R;

import java.text.DateFormatSymbols;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Locale;
import java.util.TimeZone;

/**
 * A month picker.
 */
// TODO(crbug.com/635567): Fix this properly.
@SuppressLint("DefaultLocale")
public class MonthPicker extends TwoFieldDatePicker {
    private static final int MONTHS_NUMBER = 12;

    private final String[] mShortMonths;

    public MonthPicker(Context context, double minValue, double maxValue) {
        super(context, minValue, maxValue);

        getPositionInYearSpinner().setContentDescription(
                getResources().getString(R.string.accessibility_date_picker_month));

        // initialization based on locale
        mShortMonths =
                DateFormatSymbols.getInstance(Locale.getDefault()).getShortMonths();

        // logic duplicated from android.widget.DatePicker
        if (usingNumericMonths()) {
            // We're in a locale where a date should either be all-numeric, or all-text.
            // All-text would require custom NumberPicker formatters for day and year.
            for (int i = 0; i < mShortMonths.length; ++i) {
                mShortMonths[i] = String.format("%d", i + 1);
            }
        }

        // initialize to current date
        Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        init(cal.get(Calendar.YEAR), cal.get(Calendar.MONTH), null);
    }

    /**
     * Tests whether the current locale is one where there are no real month names,
     * such as Chinese, Japanese, or Korean locales.
     */
    protected boolean usingNumericMonths() {
        return Character.isDigit(mShortMonths[Calendar.JANUARY].charAt(0));
    }

    /**
     * Creates a date object from the |value| which is months since epoch.
     */
    public static Calendar createDateFromValue(double value) {
        int year = (int) Math.min(value / 12 + 1970, Integer.MAX_VALUE);
        int month = (int) (value % 12);
        Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        cal.clear();
        cal.set(year, month, 1);
        return cal;
    }

    @Override
    protected Calendar getDateForValue(double value) {
        return MonthPicker.createDateFromValue(value);
    }

    @Override
    protected void setCurrentDate(int year, int month) {
        Calendar date = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        date.set(year, month, 1);
        if (date.before(getMinDate())) {
            setCurrentDate(getMinDate());
        } else if (date.after(getMaxDate())) {
            setCurrentDate(getMaxDate());
        } else {
            setCurrentDate(date);
        }
    }

    @Override
    protected void updateSpinners() {
        super.updateSpinners();

        // make sure the month names are a zero based array
        // with the months in the month spinner
        String[] displayedValues = Arrays.copyOfRange(mShortMonths,
                getPositionInYearSpinner().getMinValue(),
                getPositionInYearSpinner().getMaxValue() + 1);
        getPositionInYearSpinner().setDisplayedValues(displayedValues);
    }

    /**
     * @return The selected month.
     */
    public int getMonth() {
        return getCurrentDate().get(Calendar.MONTH);
    }

    @Override
    public int getPositionInYear() {
        return getMonth();
    }

    @Override
    protected int getMaxYear() {
        return getMaxDate().get(Calendar.YEAR);
    }

    @Override
    protected int getMinYear() {
        return getMinDate().get(Calendar.YEAR);
    }


    @Override
    protected int getMaxPositionInYear(int year) {
        if (year == getMaxDate().get(Calendar.YEAR)) {
            return getMaxDate().get(Calendar.MONTH);
        }
        return MONTHS_NUMBER - 1;
    }

    @Override
    protected int getMinPositionInYear(int year) {
        if (year == getMinDate().get(Calendar.YEAR)) {
            return getMinDate().get(Calendar.MONTH);
        }
        return 0;
    }
}
