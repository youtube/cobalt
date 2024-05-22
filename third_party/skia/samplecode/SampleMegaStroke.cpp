/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkCanvas.h"
#include "include/core/SkPath.h"
#include "include/utils/SkRandom.h"
#include "samplecode/Sample.h"

class MegaStrokeView : public Sample {
public:
    MegaStrokeView() {
        fClip.setLTRB(0, 0, 950, 600);
        fAngle = 0;
        fPlusMinus = 0;
        SkRandom rand;
        fMegaPath.reset();
        for (int index = 0; index < 921; ++index) {
            for (int segs = 0; segs < 40; ++segs) {
                fMegaPath.lineTo(SkIntToScalar(index), SkIntToScalar(rand.nextRangeU(500, 600)));
            }
        }
    }

protected:
    SkString name() override { return SkString("MegaStroke"); }

    bool onChar(SkUnichar uni) override {
        fClip.setLTRB(0, 0, 950, 600);
        return true;
    }

    void onDrawBackground(SkCanvas* canvas) override {
    }

    void onDrawContent(SkCanvas* canvas) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setARGB(255,255,153,0);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1);

        canvas->save();
        canvas->clipRect(fClip);
        canvas->clear(SK_ColorWHITE);
        canvas->drawPath(fMegaPath, paint);
        canvas->restore();

        SkPaint divSimPaint;
        divSimPaint.setColor(SK_ColorBLUE);
	    SkScalar x = SkScalarSin(fAngle * SK_ScalarPI / 180) * 200 + 250;
	    SkScalar y = SkScalarCos(fAngle * SK_ScalarPI / 180) * 200 + 250;

        if ((fPlusMinus ^= 1)) {
            fAngle += 5;
        } else {
            fAngle -= 5;
        }
        SkRect divSim = SkRect::MakeXYWH(x, y, 100, 100);
        divSim.outset(30, 30);
        canvas->drawRect(divSim, divSimPaint);
        fClip = divSim;
    }

    void onSizeChange() override {
        fClip.setWH(950, 600);
    }

    bool onAnimate(double /*nanos*/) override { return true; }

private:
    SkPath      fMegaPath;
    SkRect      fClip;
    int         fAngle;
    int         fPlusMinus;
    using INHERITED = Sample;
};

//////////////////////////////////////////////////////////////////////////////

DEF_SAMPLE( return new MegaStrokeView(); )
