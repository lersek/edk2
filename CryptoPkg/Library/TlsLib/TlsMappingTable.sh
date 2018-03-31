## @file
#
# POSIX shell script to refresh "TlsCipherMappingTable" in "TlsConfig.c".
#
# Note: the output of this script is not meant to blindly replace the current
# contents of "TlsCipherMappingTable". It only helps with the composition and
# formatting of candidate lines.
#
# Copyright (C) 2018, Red Hat, Inc.
#
# This program and the accompanying materials are licensed and made available
# under the terms and conditions of the BSD License which accompanies this
# distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
# WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
##

# Exit on error, treat unset variables as errors, don't overwrite existing
# files with the ">" output redirection operator.
set -e -u -C

# This script uses a few utilities that are not defined by POSIX. Check if they
# are available.
if ( ! command -v mktemp  ||
     ! command -v openssl ||
     ! command -v curl    ||
     ! command -v column ) >/dev/null
then
  BASENAME=$(basename -- "$0")
  {
    printf -- '%s: please install the following utilities first:\n' "$BASENAME"
    printf -- '%s:   mktemp openssl curl column\n' "$BASENAME"
  } >&2
  exit 1
fi

# Create a temporary file for saving the OpenSSL output.
OPENSSL_LIST=$(mktemp)
trap 'rm -f -- "$OPENSSL_LIST"' EXIT

# Create a temporary file for saving the IANA TLS Cipher Suite Registry.
IANA_LIST=$(mktemp)
trap 'rm -f -- "$OPENSSL_LIST" "$IANA_LIST"' EXIT

# Sorting, and range expressions in regular expressions, depend on the locale.
# Use a well-defined locale.
LC_ALL=C
export LC_ALL

# Get OPENSSL_LIST.
{
  # List cipher suite codes and names from OpenSSL.
  openssl ciphers -V ALL

  # This produces a line format like:
  # 0xC0,0x30 - ECDHE-RSA-AES256-GCM-SHA384 TLSv1.2 Kx=ECDH Au=RSA Enc=AESGCM(256) Mac=AEAD
  # (sequences of space characters squeezed for brevity).
} |
{
  # Project the list to UINT16 hex codes (network byte order interpretation)
  # and OpenSSL cipher suite names.
  sed -r -n -e 's/^ *0x([0-9A-F]{2}),0x([0-9A-F]{2}) - ([^ ]+) .*$/\1\2 \3/p'

  # This produces a line format like:
  # C030 ECDHE-RSA-AES256-GCM-SHA384
} |
{
  # Sort the output so we can later join it on the UINT16 hex code as key.
  sort
} >>"$OPENSSL_LIST"

# Get IANA_LIST.
{
  # Download the CSV file from the IANA website.
  curl -s https://www.iana.org/assignments/tls-parameters/tls-parameters-4.csv

  # This produces a line format like:
  # Value,Description,DTLS-OK,Reference
  # "0x00,0x00",TLS_NULL_WITH_NULL_NULL,Y,[RFC5246]
} |
{
  # Project the list to UINT16 hex codes (network byte order interpretation)
  # and Descriptions (TLS_xxx macros).
  sed -r -n \
    -e 's/^"0x([0-9A-F]{2}),0x([0-9A-F]{2})",([A-Za-z0-9_]+).*$/\1\2 \3/p'

  # This produces a line format like:
  # 0000 TLS_NULL_WITH_NULL_NULL
} |
{
  # Sort the output so we can later join it on the UINT16 hex code as key.
  sort
} >>"$IANA_LIST"

# Produce the C source code on stdout.
{
  # Join the two lists first. Elements that are in exactly one input file are
  # dropped.
  join -- "$OPENSSL_LIST" "$IANA_LIST"

  # This produces a line format like:
  # 0004 RC4-MD5 TLS_RSA_WITH_RC4_128_MD5
  # And the output remains sorted by UINT16 hex key.
} |
{
  # Produce a valid C language line. Be careful that only one space character
  # is preserved, for the next step.
  sed -r -n -e 's!^([0-9A-F]{4}) ([^ ]+) ([^ ]+)$!{0x\1,"\2"}, ///\3!p'

  # This produces a line format like:
  # {0x0004,"RC4-MD5"}, ///TLS_RSA_WITH_RC4_128_MD5
} |
{
  # Align the rightmost column nicely (the TLS_xxx macros). The "column"
  # command will expand the space character as necessary.
  column -t

  # This produces a line format like:
  # {0x0004,"RC4-MD5"},                        ///TLS_RSA_WITH_RC4_128_MD5
} |
{
  # Final touches:
  # - replace the opening brace "{" with "  MAP ( ",
  # - insert one space character after the first comma ","
  # - replace the closing brace "}" with " )",
  # - remove one space character after the second comma "," (because the
  #   "column" utility pads space characters to at least two),
  # - insert one space character after the comment marker "///"
  sed \
    -e 's/^{/  MAP ( /' \
    -e 's/,/, /' \
    -e 's/}, / ),/' \
    -e 's!///!/// !'

  # This produces a line format like:
  #   MAP ( 0x0004, "RC4-MD5" ),                       /// TLS_RSA_WITH_RC4_128_MD5
}
