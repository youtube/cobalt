# Cobalt Unit Test Setup on Darwin tvOS platforms

## To run unit tests on simulator or arm64 device

1. Install Xcode and put it in /Applications/. You also need to verify
   installation of xcode command line tool kit.

   Specifically, make sure `codesign` command works.

## To run unit tests on arm64 devices, you need these extra setups

1. Follow instructions here and install third party tool ios-deploy:
   https://github.com/ios-control/ios-deploy

2. Install Apple's certificate and Google's tvOS Development certificate:

   1. Download "AppleWWDRCA.cer" and "GoogleDevelopment...p12" from go/
       ioscerts and install them by clicking them;

   2. Open Mac's "Keychain Access" app. After previous step you should have a
      "iPhone Developer: Google Development" certificate in your keychain.

   3. (Optional) If this certificate is in "login" keychain but not "System"
      keychain and you want to allow another user(e.g. youtube-steel-buildbot)
      to access Google's Development Certificate(enable it to deploy to device):

      1. Add the certificate to System keychain by dragging it to "System".

      2. Click on the lock symbol on the top left of "Keychain Access" panel
         and enter your password to unlock root access;

      3. Expand the "iPhone Developer: Google..." certificate and double-click
         its private key;

      4. Go to "Access Control", click on "Allow all applications to access
         this item", enter your password if prompted and click "Save Changes".

3. Install Google Development provisioning profile to the device by either:

    1. Install the provisioning profile through Xcode's "Devices and Simulator"
       panel, please refer to step 2 of "Setting up a new Apple TV" in
       go/cobalt-apple-setup for instructions.
    2. Deploy the YouTube app. The provisioning profile it contains will be
       installed to the device along with the app.

       1. To get the app, you can either follow go/cobalt-apple-setup or use
          any other computer that has set up the YouTube app correctly;

       2. Connect to the new device and just click Run to deploy the app.
