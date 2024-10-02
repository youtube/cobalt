// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_url_generator.h"

#include "base/time/time.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace google_apis {

namespace calendar {

// Make sure the hard-coded urls are returned.
TEST(CalendarApiUrlGeneratorTest, GetColorListUrl) {
  CalendarApiUrlGenerator url_generator_;
  EXPECT_EQ("https://www.googleapis.com/calendar/v3/colors",
            url_generator_.GetCalendarColorListUrl().spec());
}

TEST(CalendarApiUrlGeneratorTest, GetEventListUrl) {
  CalendarApiUrlGenerator url_generator_;
  base::Time start;
  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 PST", &start));
  base::Time end;
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 PST", &end));
  EXPECT_EQ(
      "https://www.googleapis.com/calendar/v3/calendars/primary/"
      "events?timeMin=2021-06-13T18%3A00%3A00.000Z"
      "&timeMax=2021-06-16T18%3A00%3A00.000Z"
      "&singleEvents=true"
      "&maxAttendees=1"
      "&maxResults=123",
      url_generator_
          .GetCalendarEventListUrl(start, end, /*single_events=*/true,
                                   /*max_attendees=*/1,
                                   /*max_results=*/123)
          .spec());
}

TEST(CalendarApiUrlGeneratorTest,
     GetEventListUrlWithDefaultOptionalParameters) {
  CalendarApiUrlGenerator url_generator_;
  base::Time start;
  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 PST", &start));
  base::Time end;
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 PST", &end));
  EXPECT_EQ(
      "https://www.googleapis.com/calendar/v3/calendars/primary/"
      "events?timeMin=2021-06-13T18%3A00%3A00.000Z"
      "&timeMax=2021-06-16T18%3A00%3A00.000Z"
      "&singleEvents=true",
      url_generator_
          .GetCalendarEventListUrl(start, end, /*single_events=*/true,
                                   /*max_attendees=*/absl::nullopt,
                                   /*max_results=*/absl::nullopt)
          .spec());
}

}  // namespace calendar
}  // namespace google_apis
