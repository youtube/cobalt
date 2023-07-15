# Copyright 2023 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Contains Xbox variables for the appx manifest."""

# Required product settings used in the xml template. If any of these are
# missing, the AppxManifest.xml file will fail to generate properly.
PRODUCT_SETTINGS = {
    'cobalt': {
        # Value for the Name attribute of Identity:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity#attributes
        'IDENTITY_NAME': 'Cobalt',
        # Value for the Publisher attribute of Identity:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity#attributes
        'PUBLISHER': 'CN=CommonName',
        # Value for the DisplayName element:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-displayname
        'DISPLAY_NAME': 'Cobalt',
        # Value for the PublisherDisplayName element:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-publisherdisplayname
        'PUBLISHER_DISPLAY_NAME': 'My Company',
        # Value for the EntryPoint attribute of Application:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-application#attributes
        'ENTRYPOINT': 'https://youtube.com/tv',
        # Value for the DisplayName attribute of VisualElements:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap-visualelements#attributes
        'APPLICATION_DISPLAY_NAME': 'CobaltApp',
        # Value for the Description attribute of VisualElements:
        # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap-visualelements#attributes
        'APPLICATION_DESCRIPTION': 'The Cobalt App',
    },
}

# Optional fields used in the xml template. If any of these are included in a
# product in PRODUCT_SETTINGS above they will be used, otherwise they will be
# skipped.
OPTIONAL_PRODUCT_SETTINGS = {
    # Value for the IgnorableNamespaces attribute of Package:
    # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-package#attributes
    'IGNORABLE_NAMESPACES':
        'uap mp build',
    # XML element for PhoneIdentity:
    # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-mp-phoneidentity
    'PHONE_IDENTITY':
        '<mp:PhoneIdentity PhoneProductId="..." PhonePublisherId="..." />',
    # List of XML elements for capabilities:
    # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-capabilities
    # Will simply be inserted line by line into the template within the
    # <Capabilities> element.
    'EXTRA_CAPABILITIES': ['<Capability Name="objects3D" />'],
    # List of XML elements for extensions:
    # https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap-extension
    # Will simply be inserted line by line into the template within the
    # <Extensions> element.
    'EXTRA_EXTENSIONS': [
        '<uap:Extension Category="windows.autoPlayContent">',
        '   <uap:AutoPlayContent>'
        '     <uap:LaunchAction'
        '       Verb="show"'
        '       ActionDisplayName="Show Pictures"'
        '       ContentEvent="ShowPicturesOnArrival"/>'
        '   </uap:AutoPlayContent>'
        ' </uap:Extension>,'
    ],
}
