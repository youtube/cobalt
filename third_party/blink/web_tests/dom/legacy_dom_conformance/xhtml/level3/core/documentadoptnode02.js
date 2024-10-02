/*
Copyright Â© 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, European Research Consortium
for Informatics and Mathematics, Keio University). All
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/

   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode02";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();

      docsLoaded = 0;

      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "hc_staff");

       if (docsLoaded == 1) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
        catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}

//
//   This method is called on the completion of
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 1) {
        setUpPageStatus = 'complete';
    }
}

/**
*
    Adopt the class attribute node of the fourth acronym element.  Check if this attribute has been adopted
    successfully by verifying the nodeName, nodeType, ownerElement, specified attributes and child nodes
    of the adopted node.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode02() {
   var success;
    if(checkInitialization(builder, "documentadoptnode02") != null) return;
    var doc;
      var newDoc;
      var domImpl;
      var attrOwnerElem;
      var element;
      var attr;
      var childList;
      var adoptedclass;
      var attrsParent;
      var nodeName;
      var nodeType;
      var nodeValue;
      var isSpecified;
      var nullDocType = null;

      var docElem;
      var rootNS;
      var rootName;

      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docElem = doc.documentElement;

      rootName = docElem.tagName;

      rootNS = docElem.namespaceURI;

      domImpl = doc.implementation;
newDoc = domImpl.createDocument(rootNS,rootName,nullDocType);
      childList = doc.getElementsByTagName("acronym");
      element = childList.item(3);
      attr = element.getAttributeNode("class");
      adoptedclass = newDoc.adoptNode(attr);

    nodeName = adoptedclass.nodeName;

      nodeValue = adoptedclass.nodeValue;

      nodeType = adoptedclass.nodeType;

      attrOwnerElem = adoptedclass.ownerElement;

      isSpecified = adoptedclass.specified;

      assertEquals("documentadoptnode02_nodeName","class",nodeName);
       assertEquals("documentadoptnode02_nodeType",2,nodeType);
       assertNull("documentadoptnode02_ownerDoc",attrOwnerElem);
    assertTrue("documentadoptnode02_specified",isSpecified);
}

function runTest() {
   documentadoptnode02();
}
