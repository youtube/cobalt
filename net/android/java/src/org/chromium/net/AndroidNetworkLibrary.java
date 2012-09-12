// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.CalledByNativeUnchecked;

import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.URLConnection;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.util.Enumeration;

/**
 * This class implements net utilities required by the net component.
 */
class AndroidNetworkLibrary {

    private static final String TAG = AndroidNetworkLibrary.class.getName();

    /**
     * Stores the key pair into the CertInstaller application.
     */
    @CalledByNative
    static public boolean storeKeyPair(Context context, byte[] public_key, byte[] private_key) {
        // This is based on android.security.Credentials.install()
        // TODO(joth): Use KeyChain API instead of hard-coding constants here:
        // http://crbug.com/124660
        try {
            Intent intent = new Intent("android.credentials.INSTALL");
            intent.setClassName("com.android.certinstaller",
                    "com.android.certinstaller.CertInstallerMain");
            intent.putExtra("KEY", private_key);
            intent.putExtra("PKEY", public_key);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "could not store certificate: " + e);
        }
        return false;
    }

    /**
     * @return the mime type (if any) that is associated with the file
     *         extension. Returns null if no corresponding mime type exists.
     */
    @CalledByNative
    static public String getMimeTypeFromExtension(String extension) {
        return URLConnection.guessContentTypeFromName("foo." + extension);
    }

    /**
     * @return true if it can determine that only loopback addresses are
     *         configured. i.e. if only 127.0.0.1 and ::1 are routable. Also
     *         returns false if it cannot determine this.
     */
    @CalledByNative
    static public boolean haveOnlyLoopbackAddresses() {
        Enumeration<NetworkInterface> list = null;
        try {
            list = NetworkInterface.getNetworkInterfaces();
            if (list == null) return false;
        } catch (SocketException e) {
            Log.w(TAG, "could not get network interfaces: " + e);
            return false;
        }

        while (list.hasMoreElements()) {
            NetworkInterface netIf = list.nextElement();
            try {
                if (netIf.isUp() && !netIf.isLoopback()) return false;
            } catch (SocketException e) {
                continue;
            }
        }
        return true;
    }

    /**
     * @return the network interfaces list (if any) string. The items in
     *         the list string are delimited by a semicolon ";", each item
     *         is a network interface name and address pair and formatted
     *         as "name,address". e.g.
     *           eth0,10.0.0.2;eth0,fe80::5054:ff:fe12:3456
     *         represents a network list string which containts two items.
     */
    @CalledByNative
    static public String getNetworkList() {
        Enumeration<NetworkInterface> list = null;
        try {
            list = NetworkInterface.getNetworkInterfaces();
            if (list == null) return "";
        } catch (SocketException e) {
            Log.w(TAG, "Unable to get network interfaces: " + e);
            return "";
        }

        StringBuilder result = new StringBuilder();
        while (list.hasMoreElements()) {
            NetworkInterface netIf = list.nextElement();
            try {
                // Skip loopback interfaces, and ones which are down.
                if (!netIf.isUp() || netIf.isLoopback())
                    continue;
                Enumeration<InetAddress> addressList = netIf.getInetAddresses();
                while (addressList.hasMoreElements()) {
                    InetAddress address = addressList.nextElement();
                    // Skip loopback addresses configured on non-loopback interfaces.
                    if (address.isLoopbackAddress())
                        continue;
                    StringBuilder addressString = new StringBuilder();
                    addressString.append(netIf.getName());
                    addressString.append(",");

                    String ipAddress = address.getHostAddress();
                    if (address instanceof Inet6Address && ipAddress.contains("%")) {
                        ipAddress = ipAddress.substring(0, ipAddress.lastIndexOf("%"));
                    }
                    addressString.append(ipAddress);

                    if (result.length() != 0)
                        result.append(";");
                    result.append(addressString.toString());
                }
            } catch (SocketException e) {
                continue;
            }
        }
        return result.toString();
    }

    /**
     * Validate the server's certificate chain is trusted.
     *
     * @param certChain The ASN.1 DER encoded bytes for certificates.
     * @param authType The key exchange algorithm name (e.g. RSA)
     * @return true if the server is trusted
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException
     *             on error initializing the TrustManager or reading the
     *             certChain
     */
    @CalledByNativeUnchecked
    public static boolean verifyServerCertificates(byte[][] certChain, String authType)
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        return X509Util.verifyServerCertificates(certChain, authType);
    }

}
