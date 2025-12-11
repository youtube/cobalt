/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmllite/xmlprinter.h"

#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/qname.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmllite/xmlnsstack.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;
using jingle_xmpp::XmlnsStack;
using jingle_xmpp::XmlPrinter;

TEST(XmlPrinterTest, TestBasicPrinting) {
  XmlElement elt(QName("google:test", "first"));
  std::stringstream ss;
  XmlPrinter::PrintXml(&ss, &elt);
  EXPECT_EQ("<test:first xmlns:test=\"google:test\"/>", ss.str());
}

TEST(XmlPrinterTest, TestNamespacedPrinting) {
  XmlElement elt(QName("google:test", "first"));
  elt.AddElement(new XmlElement(QName("nested:test", "second")));
  std::stringstream ss;

  XmlnsStack ns_stack;
  ns_stack.AddXmlns("gg", "google:test");
  ns_stack.AddXmlns("", "nested:test");

  XmlPrinter::PrintXml(&ss, &elt, &ns_stack);
  EXPECT_EQ("<gg:first><second/></gg:first>", ss.str());
}
