This directory contains a cobalt.pfx cert for signing UWP appx packages.

This cert is not for use with submitting to the Microsoft store. It
is intended only to be used for running the Windows App Cert Kit. Note that
you will need to regenerate this file to be able to sign a cobalt appx yourself.

It was generated as follows, using tools in SDK 10.0.22621.0 run in PowerShell
as an administrator:

Create a new self-signed certificate with an extended key usage for code
signing. The Subject must match the Publisher field in your AppxManifest.
`New-SelfSignedCertificate -Type Custom -Subject "<Publisher information from AppxManifest.xml>" -FriendlyName "cobalt-cert" -KeyUsage DigitalSignature -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")`

Verify that the cert was created properly. You should see the new cert if you
run the following command (Cert:\LocalMachine\My is the default cert store,
yours may be in a different location).
`Get-ChildItem Cert:\LocalMachine\My | Format-Table Subject, FriendlyName, Thumbprint`

Export the certificate to a Personal Information Exchange (pfx) file.
`Export-PfxCertificate -cert Cert:\LocalMachine\My\<Certificate Thumbprint> -FilePath <FilePath>.pfx -ProtectTo <Username or group name>`

See the following for more information:

https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing

It is recommended that you remove any certificates once they are no longer
necessary to prevent them from being used maliciously. If you need to remove
this certificate, run the following in PowerShell as an administrator.

`Get-ChildItem Cert:\LocalMachine\My | Format-Table Subject, FriendlyName, Thumbprint`

`Get-ChildItem Cert:\LocalMachine\My\<Certificate Thumbprint> | Remove-Item`
