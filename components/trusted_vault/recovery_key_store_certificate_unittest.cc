// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_certificate.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/trusted_vault/securebox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

static constexpr std::string_view kCertXml =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<certificate>
  <metadata>
    <serial>10016</serial>
    <creation-time>1694037058</creation-time>
    <refresh-interval>2592000</refresh-interval>
    <previous>
      <serial>10015</serial>
      <hash>TQudrujnu1I9bdoDaYxGQYuRN/8SwTLjdk6vzYTOkIU=</hash>
    </previous>
  </metadata>
  <intermediates>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
  </intermediates>
  <endpoints>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMCD3sSR26q9occ1Y/K2SQyIsSJkJtGALvd3t4l9E8ajmOV9fQHp7d4ExmRJIldlFL/Y5i5FBg3NvwK7TLvoAPmjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAD7HLz0sS04rV7BXzrd2KJdMk2fCbrjTPNNUUZu+UbPB0lDvWcP1+uroIOEZuPLUK0EBbQYzCjP/bp7tT4me4myivPbg2IBLvTaOVKbUzi6SqA4X+vyAe3c7Bp6A3hPzxNangk2jmpKdIvLXJ8DHyXVrCXk/dNObnWUDnvbmoXg5yWK/snB5OIysDPUlxUmRspxhRajVgRnDAMTnJ2YZhHC15Jm/neugxVKeSeBb4wamLRibkdWbc4KJTiSjh1CnH1OKsCI8N006Gk+YXHnrY3OmakVg/bSnfAoMWLMDvtXbDbMVYAl9uRLBDwoOS6MFMsrj+Iwniuv4E2Kb+UcWK36AR/KH1/ILFpRUTtfPwIQcvEc2tWkH+W2BJqKOvwGH3rOm2qF88g8/egrHua7jnv8aJlfQ3c3S7ytikxugCQhSAJhVO0kdWXGUut78UzBrhMEvBqHlQtZnyPSEWd6bJKdGqwmbQwdKoou5HCu0YQxanmzENR9PmDs6+AMN0xJDcb9TOBQsvQW+vY3D34U61izaU2xytglgRzjSlBwFYDP75VgsL9gcNlYSt9R1EroPPsaEV1xhW47WpWArLdprVhVX70kPf3fUkcpDXimapFpMWONWlSUCIKPy/q0d2DcamL9HN5sZLyOGPctMTEowPomW8TiISWJFdtSK2fJXkk8s</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOHSWq/RFpU1VnCCCmPcTDeJT3t3+27+BjFOdsC8/hcnbFUKwHt6Tt0uiHV3LP/aO0/DHYC8Kdb/KAMC+ai+aJ2jEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBALz6PK44f46capH7isFvHMdTosG3DIV4QP70zLtGtGBM+57RKU0UYLtgdtKfCCwQVIgru9PfMdNdbxKojI96cfB/QxsH5H/96iUET+EnvvQ63NMSnLtOp7H4UceBujpXeSLN0yRNr59JS+mLtyL5+5KjHgtOM7tpxJ3eP1tx8NnE30TE0BoeTQyoKu0wfHVsc5+Fs3EWJUpgV+Z0/KJFoy3M2Z0DHZxfn6fg+/xYxn8ttkMhlZXhJMjNqtcGmlwLYktmsG5LlsQNimXwGl9olVviEZwcHGUzHw8QWszoKzn+TgTgv76m2eZ5MwJeN1JnaLb+1gQtgKRpnG8TFxWGC/TIHUqLow/GruH2TSlLPr6l6ed+QjG01sAN5cdI7OR84D8W1F0vb8fVOr7kjf7N3qLDNQXDCRUUKHlRVanIt6h+kT1ctlM51+QmRhDsAkzY/3lFrXDySnQk18vlzTyA+QgqmvfNkPhgCp/fpgtWJFaPL9bJWaMaW/soXRUf26F6RMLK43EihdoVMtUAvmCIKUQyI88X6hJxEhWLyy/8Y45nAFk5CgXuzV2doOJTSITtJligTy1IuczH75bmp87c5ZPp51vUO4WYXuwffTCoQ8UYSYbNxxqKOfFkILnM1WoGAzCrVt5aKOyGPILzOsOS8X0EeQ9YF6Mvaf2iFljc2o30</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABNeVqPpEctoVzN48WNefTpJEmRrrbpXoWRhHwH/AOYmQgXR6xX/AE1/qeen8fMj4Lnyb8KPveZjXvTlFq2mdBHGjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAEQIGwhKa7MDq+Wt5p7fvv1AXhX4HxpgkKv5xbuMWCcw6R8zTYQ4hF/XHegIEqjmwWFxEvD95Lu3oLz4gMEoZVywBt2QFb1wkWUjdeT9oy5YbrJiLm9evhMFWyjnu2h9OVqxCVvarVx35ZySThDr2n3CYntLSKyTSdVlzCsdcCOj1UFkqMe73gOUZFMkXETUoINlFYwX6NP5V1Moy8OjsSNa6/8zyYwivm3rQlj3GUEhSlX+0ib+IXYpcrDFF7/6+G8lWBAHmKGwGR6kpAQ7Zg7KEjY0gSYWOr86oJIMFzeXVjaqhwGXK2tO+JBTPZSf4zljke+QCDN1uZjscgpOOXcBvT3LqLDaz2TSen4EMXhD56lYrq/970a1ol7B26nNAjJr1Q2ZyH4kXgBnK/b7AjYzNhTx0k0o7zRdh4tMeNkxhHgpBQ7d8VM81lZJg95n5SuOvJkJlEsPus9nJ1QeKAAjLV+Hp4n+xEImnvwnPEeE9vo07KHeHsCaBFVVan+9VKMiFEnYO+JdA8DwVTwTHHRH2T2OcEF+oo6m9nZZgGZbcovftryoOetJRY8E2JG+j5ScVWwnh5QcWhP1oOqsZdFWbKmJyxbN0qhKRWB1l6xZipMTj4RYzrZtwXNWdJIudC1Lkr6GgMn2UybLPc4xDH5FLWDtLN7griLweFrniuAQ</cert>
  </endpoints>
</certificate>
)";

static constexpr std::string_view kSigXml =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
  </intermediates>
  <certificate>MIIFGTCCAwGgAwIBAgIRAOUOMMnP/H98t0zAwO3YjxIwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUxMDBaFw0yODA5MDYyMTUxMDBaMDUxMzAxBgNVBAMTKkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBTaWduaW5nIEtleTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANqoaDjGHUrdnO6raw9omQ+xnhSxqwTSY2dlC83an+F9JNlL/CHjvn+kyKP7rP57k4y9+9REqjvk+zaR6rQjzP6m2FbYf/kXsmS8ohtTXsmI9NTvobGCGZOYwFbB28yxoOiXA2A91cG+Rt/KmetMcGphFE0/9PGZg9JSmWiGLDJEvgG4ckz6fmL/orhbC/V1K3ArNZ2eJ8Sw29eMo62XpJqvmi+6BrFS3edcJNC1dUpC/ixP73G1J5XDVb60no4JolG1N7Utug/WlPr88eI7LdV05sMfRfX+ta4TrIK7yJ1urGuOVsIDBGFjsfgpRTlwiG829D9uGhRSAE8GzVCFiVF8AfQwlEtgahwg23QzWRaKYo6qeRMCw1hNURF31hQ5bgQeKcaS98x6MkzszBOT2aFiK0EWBzwsJLI3KadRYUMcKa3AFXSv7QLGkAU+Ivas/m3Mt0s7KQnIzjsYbOqiC895WsylxaQyMy5xvVKp0gYjmK2YtgfXo59hznqns1FzeR4fBsbKsh+NnWXzcJ8cEg8jbk0nxAz0reMj1IN25Wb1WDfUCiTy+9V6dfFLQFQ6KYDb/bbIRyPk4g176gWK9agVrHrhiQsDVstSN/cAgLBVUFi1oeLzZ0SwB4wCXuP8SmEVrGl3zxxv3szgUxwfm+elaZ0BrA5deSenJdhV1QQ3AgMBAAGjIDAeMA4GA1UdDwEB/wQEAwIHgDAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4ICAQDuLSK5nov/grmYNc8CTnrKNZ1w8p5Wbi9QThzJXoSV1BuFklXNX4GlgjZ04eS5ns/lUCdqByx0K2ZGX24wzZX0sSUQ+74Fq5uDINm6ESPV46y6hXvqIotLYIrgpl7Z2Ej7D6JT5fPYzAncUQd8Z9LuNMMt/rG8IlfSN6yOuZnAxI8wKtCrp23QugtqYKHyfxCN/HzCMEs1XP7qhgolnmLoTqU9j2HlPPESmH4+St4w7QPVQWARQ2S0hdtT4dhjmkqeDBojBjkGn9fS+vsOKsH3CDTt3A0pFI66xQ9TwT5mHCIIkAxGzc/DzPtpTUz6XBhtWNyI59adbCHfOtWWNjpriYvTbOm1ZZL6DXsaFJIbYX0Cmh6unonuvZ2c1Pu6nnVxR1HamIdtDZjvgbyFRJ4wCWpMhAU9WVJSotz57OXf/CvbBI0gfhl/EmWtKsGiDryPjphILWrnO55V6G6HJgk6xpzcjZzSnWpf5UF9RGjUaZNwOtxma/57pM8o5vTCeaOrq/3dKUWO2JBgxkOG+/ZCOe0E0Q2CwCCWTtf4ReaUIbeYQTj4cfR4eaj6Z8euytwEM2UQCep+HXJdOxv6/eHRXPK21Alt0crWmhZ8J7hZyeZ/24a3in8hqg9X9wxZXPghXo4W3My3Tn+dP2m36RiBQOCHSoYWMRINZccj9284GQ==</certificate>
  <value>n6kI2dGZKz5CGbXnbz79m51QTDt+WszzNOvcqXsGm6g3ObmpjkghTU3wPmrJ0c5zUD1l4QQEmTKRBIACgK7Sp64JdC4IGP5y+z8HhXPslP3Dc5aySOk4b++m7AIbkAuw63SbPD8L2nQ20CMNiaVVBqZJ0uWUV04qN8IOll1L8NbeZLhjFUcx9riYBrzWOr9uis5IANkfPTFgFyPFjqFk9XrbVpPcNCRtz7Pew+L7OW5z7sh5rW8iZmjhhV/e4VDTgYBFq/Js5W4yalRI9uuEXLJqG1/US4L5cMnJoZOxPmz48an0ug/Pi8yV9cIq+xvER/XaeeUG53Fqy9cn2qG6ROwxH109toaLx3TZaLjdVh7wcJCLtOY6WngHksQbIyU1mDYzz7uWItCss2Nb0NbZ+QMn3k1GxDGIwlY/HXdt7OihPQWLRM2H/QRqlI9p8i1L+DaPrhyGrGHzYKN8z9qGZYx1AsQUWQCR0YeXvlxjtSvBEPtWkfEE0RrZPJtFh+bvrD55Id7XapnGKKXYMmYf9KbDJ3GMD1aT6xgMhlAhtltN5vNg08LSH5Ma4TXhmNpKny5JQqlAUTby1wIhgdElQSdU0jYpmle8N0wsuLoX+e3bHFKxWVkrwvXDC0v2wqH5mzm8FLhxXZDA2ApnGT+eOC1gjd8qTuouzm5GuMhjvig=</value>
</signature>
)";

// The EC public keys are in uncompressed form. These have been extracted using
// cyberchef.
static constexpr uint8_t kEndpoint1PublicKey[] = {
    0x04, 0xc0, 0x83, 0xde, 0xc4, 0x91, 0xdb, 0xaa, 0xbd, 0xa1, 0xc7,
    0x35, 0x63, 0xf2, 0xb6, 0x49, 0x0c, 0x88, 0xb1, 0x22, 0x64, 0x26,
    0xd1, 0x80, 0x2e, 0xf7, 0x77, 0xb7, 0x89, 0x7d, 0x13, 0xc6, 0xa3,
    0x98, 0xe5, 0x7d, 0x7d, 0x01, 0xe9, 0xed, 0xde, 0x04, 0xc6, 0x64,
    0x49, 0x22, 0x57, 0x65, 0x14, 0xbf, 0xd8, 0xe6, 0x2e, 0x45, 0x06,
    0x0d, 0xcd, 0xbf, 0x02, 0xbb, 0x4c, 0xbb, 0xe8, 0x00, 0xf9};
static constexpr uint8_t kEndpoint2PublicKey[] = {
    0x04, 0xe1, 0xd2, 0x5a, 0xaf, 0xd1, 0x16, 0x95, 0x35, 0x56, 0x70,
    0x82, 0x0a, 0x63, 0xdc, 0x4c, 0x37, 0x89, 0x4f, 0x7b, 0x77, 0xfb,
    0x6e, 0xfe, 0x06, 0x31, 0x4e, 0x76, 0xc0, 0xbc, 0xfe, 0x17, 0x27,
    0x6c, 0x55, 0x0a, 0xc0, 0x7b, 0x7a, 0x4e, 0xdd, 0x2e, 0x88, 0x75,
    0x77, 0x2c, 0xff, 0xda, 0x3b, 0x4f, 0xc3, 0x1d, 0x80, 0xbc, 0x29,
    0xd6, 0xff, 0x28, 0x03, 0x02, 0xf9, 0xa8, 0xbe, 0x68, 0x9d};
static constexpr uint8_t kEndpoint3PublicKey[] = {
    0x04, 0xd7, 0x95, 0xa8, 0xfa, 0x44, 0x72, 0xda, 0x15, 0xcc, 0xde,
    0x3c, 0x58, 0xd7, 0x9f, 0x4e, 0x92, 0x44, 0x99, 0x1a, 0xeb, 0x6e,
    0x95, 0xe8, 0x59, 0x18, 0x47, 0xc0, 0x7f, 0xc0, 0x39, 0x89, 0x90,
    0x81, 0x74, 0x7a, 0xc5, 0x7f, 0xc0, 0x13, 0x5f, 0xea, 0x79, 0xe9,
    0xfc, 0x7c, 0xc8, 0xf8, 0x2e, 0x7c, 0x9b, 0xf0, 0xa3, 0xef, 0x79,
    0x98, 0xd7, 0xbd, 0x39, 0x45, 0xab, 0x69, 0x9d, 0x04, 0x71};

static constexpr base::Time kValidCertificateDate =
    base::Time::FromSecondsSinceUnixEpoch(1740614400);

std::string_view GetCertificate() {
  return std::string_view(std::begin(kSigXml) + 3615,
                          std::begin(kSigXml) + 5363);
}

std::string_view GetSignature() {
  return std::string_view(std::begin(kSigXml) + 5387,
                          std::begin(kSigXml) + 6071);
}

std::string_view GetIntermediate1() {
  return std::string_view(std::begin(kCertXml) + 354,
                          std::begin(kCertXml) + 2082);
}

std::string_view GetIntermediate2() {
  return std::string_view(std::begin(kCertXml) + 2100,
                          std::begin(kCertXml) + 3848);
}

std::string_view GetEndpoint1() {
  return std::string_view(std::begin(kCertXml) + 3899,
                          std::begin(kCertXml) + 5007);
}

std::string_view GetEndpoint2() {
  return std::string_view(std::begin(kCertXml) + 5025,
                          std::begin(kCertXml) + 6133);
}

std::string_view GetEndpoint3() {
  return std::string_view(std::begin(kCertXml) + 6151,
                          std::begin(kCertXml) + 7259);
}

class RecoveryKeyStoreCertificateTest : public testing::Test {};

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ParseValidCertXml) {
  auto result = internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints,
              testing::UnorderedElementsAre(GetEndpoint1(), GetEndpoint2(),
                                            GetEndpoint3()));
  EXPECT_THAT(
      result->intermediates,
      testing::UnorderedElementsAre(GetIntermediate1(), GetIntermediate2()));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlWithWrongRootTag) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML("<not-cert></not-cert>"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlIgnoresEmptyCerts) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <cert></cert>
    <cert>intermediate</cert>
  </intermediates>
  <endpoints>
    <cert>endpoint</cert>
    <cert></cert>
  </endpoints>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_CertXmlIgnoresUnrecognizedTags) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <something-else></something-else>
  <another-thing/>
  <intermediates>
    <not-cert></not-cert>
    <cert>intermediate</cert>
    <also-not-cert/>
  </intermediates>
  <endpoints>
    <not-cert></not-cert>
    <cert>endpoint</cert>
    <also-not-cert/>
  </endpoints>
  <something-else></something-else>
  <another-thing/>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlIgnoresNestedCerts) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <should-ignore-this>
      <cert>intermediate-should-be-ignored</cert>
    </should-ignore-this>
    <cert>intermediate</cert>
    <also-not-cert/>
  </intermediates>
  <endpoints>
    <should-ignore-this>
      <cert>endpoint-should-be-ignored</cert>
    </should-ignore-this>
    <cert>endpoint</cert>
  </endpoints>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlNoEndpoints) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
</certificate>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlNoIntermediates) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
  </intermediates>
  <endpoints>
    <cert>endpoint</cert>
  </endpoints>
</certificate>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlEmptyString) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(""));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ParseValidSigXml) {
  auto result = internal::ParseRecoveryKeyStoreSigXML(kSigXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(
      result->intermediates,
      testing::UnorderedElementsAre(GetIntermediate1(), GetIntermediate2()));
  EXPECT_EQ(result->certificate, GetCertificate());
  EXPECT_EQ(result->signature, GetSignature());
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlWithWrongRootTag) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML("<not-sig></not-sig>"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlIgnoresEmptyCerts) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert></cert>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_SigXmlIgnoresUnrecognizedTags) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <something-else></something-else>
    <cert>intermediate</cert>
    <something-else/>
  </intermediates>
  <unexpected></unexpected>
  <certificate>certificate</certificate>
  <value>signature</value>
  <unexpected/>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlIgnoresNestedCerts) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <nested>
      <cert>should ignore this</cert>
    </nested>
    <cert>intermediate</cert>
  </intermediates>
  <unexpected></unexpected>
  <certificate>certificate</certificate>
  <value>signature</value>
  <unexpected/>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoIntermediates) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoCertificate) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoSignature) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlEmptyString) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(""));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureChainSuccess) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate);
  EXPECT_TRUE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainCertificateNotB64) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain("not base 64", intermediates,
                                     kValidCertificateDate);
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainSkipsNotB64Intermediates) {
  std::vector<std::string> intermediates{"not base 64",
                                         std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate);
  EXPECT_TRUE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureChainExpired) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate + base::Days(10000));
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainNotYetValid) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate - base::Days(10000));
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignature) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  EXPECT_TRUE(internal::VerifySignature(std::move(certificate), kCertXml,
                                        GetSignature()));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureNotValid) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  std::string invalid_signature(GetSignature());
  invalid_signature[0] = 'X';
  EXPECT_FALSE(internal::VerifySignature(std::move(certificate), kCertXml,
                                         invalid_signature));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureNotBase64) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  EXPECT_FALSE(internal::VerifySignature(std::move(certificate), kCertXml,
                                         "not base 64"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ExtractEndpointPKs) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  ASSERT_EQ(pks.size(), 3u);
  EXPECT_THAT(pks.at(0)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint1PublicKey));
  EXPECT_THAT(pks.at(1)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint2PublicKey));
  EXPECT_THAT(pks.at(2)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint3PublicKey));
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_ExtractEndpointIgnoresBadCerts) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  cert_xml.endpoints.emplace_back("aabbcc");
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  EXPECT_EQ(pks.size(), 3u);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_ExtractEndpointIgnoresBadBase64) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  cert_xml.endpoints.emplace_back("not base 64");
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  EXPECT_EQ(pks.size(), 3u);
}

TEST_F(RecoveryKeyStoreCertificateTest, ParseSuccess) {
  std::optional<RecoveryKeyStoreCertificate> recovery_key_store_cert =
      RecoveryKeyStoreCertificate::Parse(kCertXml, kSigXml,
                                         kValidCertificateDate);
  ASSERT_TRUE(recovery_key_store_cert.has_value());
  ASSERT_EQ(recovery_key_store_cert->endpoint_public_keys().size(), 3u);
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(0)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint1PublicKey));
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(1)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint2PublicKey));
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(2)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint3PublicKey));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadCertXml) {
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse("not cert xml", kSigXml,
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadSigXml) {
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(kCertXml, "not sig xml",
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadChain) {
  static constexpr std::string_view kBadSigXml =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(kCertXml, kBadSigXml,
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadSignature) {
  std::string sig_xml_bad_signature(kSigXml);
  size_t signature_index = sig_xml_bad_signature.find(GetSignature());
  sig_xml_bad_signature[signature_index] = 'X';
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(
      kCertXml, sig_xml_bad_signature, kValidCertificateDate));
}

}  // namespace

}  // namespace trusted_vault
