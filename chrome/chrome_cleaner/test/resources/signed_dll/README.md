### Signed test files

This directory contains a self-signed test certificate and its private key.

1. cleaner_test_cert.crt and cleaner_test_key.key were generated with:

   ```
   openssl req -new -newkey rsa:4096 -x509 -sha256 -days 1095 -nodes -out cleaner_test_cert.crt -keyout cleaner_test_key.key
   ```

   Leave every entry blank except CN set to "Cleaner Test Cert".

   openssl is bundled with Windows Git. A copy can be found in
   depot_tools/win_tools-2_7_6_bin/git.

1. cleaner_test_cert.pfx was generated with:

   ```
   openssl pkcs12 -in cleaner_test_cert.crt -inkey cleaner_test_key.key -export -out cleaner_test_cert.pfx
   ```

   Under Windows Git's bash shell, you need to use the "winpty" wrapper because
   it asks for user input. Leave the password blank.

1. signed_empty_dll.dll was generated using the
   //chrome/chrome_cleaner/test:empty_dll target, and then signed with

   ```
   signtool.exe sign /fd sha256 /f cleaner_test_cert.pfx signed_empty_dll.dll
   ```

   A copy of signtool.exe can be found in depot_tools/win_toolchain/vs_files.

   Under Windows Git's bash shell, don't forget to escape the arguments (use
   "//fd" and "//f"). The error message if you don't escape them is cryptic.
