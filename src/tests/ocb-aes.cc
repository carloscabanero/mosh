/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

/* Test suite for the OCB-AES reference implementation included with Mosh.

   This tests cryptographic primitives implemented by others.  It uses the
   same interfaces and indeed the same compiled object code as the Mosh
   client and server.  It does not particularly test any code written for
   the Mosh project. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "ae.h"
#include "crypto.h"
#include "prng.h"
#include "fatal_assert.h"
#include "test_utils.h"

#define KEY_LEN   16
#define NONCE_LEN 12
#define TAG_LEN   16

using Crypto::AlignedBuffer;

bool verbose = false;

static bool equal( const AlignedBuffer &a, const AlignedBuffer &b ) {
  return ( a.len() == b.len() )
    && !memcmp( a.data(), b.data(), a.len() );
}

static AlignedBuffer *get_ctx( const AlignedBuffer &key ) {
  AlignedBuffer *ctx_buf = new AlignedBuffer( ae_ctx_sizeof() );
  fatal_assert( ctx_buf );
  fatal_assert( AE_SUCCESS == ae_init( (ae_ctx *)ctx_buf->data(), key.data(), key.len(), NONCE_LEN, TAG_LEN ) );
  return ctx_buf;
}

static void scrap_ctx( AlignedBuffer *ctx_buf ) {
  fatal_assert( AE_SUCCESS == ae_clear( (ae_ctx *)ctx_buf->data() ) );
  delete ctx_buf;
}

static void test_encrypt( const AlignedBuffer &key, const AlignedBuffer &nonce,
			  const AlignedBuffer &plaintext, const AlignedBuffer &assoc,
			  const AlignedBuffer &expected_ciphertext ) {
  AlignedBuffer *ctx_buf = get_ctx( key );
  ae_ctx *ctx = (ae_ctx *)ctx_buf->data();

  AlignedBuffer observed_ciphertext( plaintext.len() + TAG_LEN );

  const int ret = ae_encrypt( ctx, nonce.data(),
                              plaintext.data(), plaintext.len(),
                              assoc.data(), assoc.len(),
                              observed_ciphertext.data(), NULL,
                              AE_FINALIZE );

  if ( verbose ) {
    printf( "ret %d\n", ret );
    hexdump(observed_ciphertext, "obs ct" );
  }

  fatal_assert( ret == int( expected_ciphertext.len() ) );
  fatal_assert( equal( expected_ciphertext, observed_ciphertext ) );

  scrap_ctx( ctx_buf );
}

static void test_decrypt( const AlignedBuffer &key, const AlignedBuffer &nonce,
			  const AlignedBuffer &ciphertext, const AlignedBuffer &assoc,
			  const AlignedBuffer &expected_plaintext,
			  bool valid ) {
  AlignedBuffer *ctx_buf = get_ctx( key );
  ae_ctx *ctx = (ae_ctx *)ctx_buf->data();

  AlignedBuffer observed_plaintext( ciphertext.len() - TAG_LEN );

  const int ret = ae_decrypt( ctx, nonce.data(),
                              ciphertext.data(), ciphertext.len(),
                              assoc.data(), assoc.len(),
                              observed_plaintext.data(), NULL,
                              AE_FINALIZE );

  if ( verbose ) {
    printf( "ret %d\n", ret );
  }

  if ( valid ) {
    if ( verbose ) {
      hexdump( observed_plaintext, "obs pt" );
    }
    fatal_assert( ret == int( expected_plaintext.len() ) );
    fatal_assert( equal( expected_plaintext, observed_plaintext ) );
  } else {
    fatal_assert( ret == AE_INVALID );
  }

  scrap_ctx( ctx_buf );
}

static void test_vector( const char *key_p, const char *nonce_p,
			 size_t assoc_len,      const char *assoc_p,
			 size_t plaintext_len,  const char *plaintext_p,
			 size_t ciphertext_len, const char *ciphertext_p ) {

  AlignedBuffer key       ( KEY_LEN,        key_p );
  AlignedBuffer nonce     ( NONCE_LEN,      nonce_p );
  AlignedBuffer plaintext ( plaintext_len,  plaintext_p );
  AlignedBuffer assoc     ( assoc_len,      assoc_p );
  AlignedBuffer ciphertext( ciphertext_len, ciphertext_p );

  if ( verbose ) {
    hexdump( key, "key" );
    hexdump( nonce, "nonce" );
    hexdump( assoc, "assoc" );
    hexdump( plaintext, "exp pt" );
    hexdump( ciphertext, "exp ct" );
  }

  test_encrypt( key, nonce, plaintext, assoc,
                ciphertext );

  test_decrypt( key, nonce, ciphertext, assoc,
                plaintext, true );

  /* Try some bad ciphertexts and make sure they don't validate. */
  PRNG prng;
  for ( size_t i=0; i<64; i++ ) {
    AlignedBuffer bad_ct( ciphertext.len(), ciphertext.data() );
    ( (uint8_t *) bad_ct.data() )[ prng.uint32() % bad_ct.len() ]
      ^= ( 1 << ( prng.uint8() % 8 ) );
    test_decrypt( key, nonce, bad_ct, assoc,
                  plaintext, false );
  }

  if (verbose) {
    printf( "PASSED\n\n" );
  }
}

#define TEST_VECTOR( _key, _nonce, _assoc, _pt, _ct ) \
  test_vector( _key, _nonce, sizeof(_assoc)-1, _assoc, sizeof(_pt)-1, _pt, sizeof(_ct)-1, _ct )

static void test_all_vectors( void ) {
  /* Test vectors from http://tools.ietf.org/html/draft-krovetz-ocb-03#appendix-A */

  const char ietf_key[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F";
  const char ietf_nonce[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B";

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""  /* associated data */
    , ""  /* plaintext */
      /* ciphertext including tag */
    , "\x19\x7B\x9C\x3C\x44\x1D\x3C\x83\xEA\xFB\x2B\xEF\x63\x3B\x91\x82" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07"
    , "\x00\x01\x02\x03\x04\x05\x06\x07"
    , "\x92\xB6\x57\x13\x0A\x74\xB8\x5A\x16\xDC\x76\xA4\x6D\x47\xE1\xEA"
      "\xD5\x37\x20\x9E\x8A\x96\xD1\x4E" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07"
    , ""
    , "\x98\xB9\x15\x52\xC8\xC0\x09\x18\x50\x44\xE3\x0A\x6E\xB2\xFE\x21" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07"
    , "\x92\xB6\x57\x13\x0A\x74\xB8\x5A\x97\x1E\xFF\xCA\xE1\x9A\xD4\x71"
      "\x6F\x88\xE8\x7B\x87\x1F\xBE\xED" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\x77\x6C\x99\x24\xD6\x72\x3A\x1F\xC4\x52\x45\x32\xAC\x3E\x5B\xEB" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
    , ""
    , "\x7D\xDB\x8E\x6C\xEA\x68\x14\x86\x62\x12\x50\x96\x19\xB1\x9C\xC6" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\x13\xCC\x8B\x74\x78\x07\x12\x1A\x4C\xBB\x3E\x4B\xD6\xB4\x56\xAF");

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xFC\xFC\xEE\x7A\x2A\x8D\x4D\x48\x5F\xA9\x4F\xC3\xF3\x88\x20\xF1"
      "\xDC\x3F\x3D\x1F\xD4\xE5\x5E\x1C" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , ""
    , "\x28\x20\x26\xDA\x30\x68\xBC\x9F\xA1\x18\x68\x1D\x55\x9F\x10\xF6" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xFC\xFC\xEE\x7A\x2A\x8D\x4D\x48\x6E\xF2\xF5\x25\x87\xFD\xA0\xED"
      "\x97\xDC\x7E\xED\xE2\x41\xDF\x68" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xFC\xFC\xEE\x7A\x2A\x8D\x4D\x48\x5F\xA9\x4F\xC3\xF3\x88\x20\xF1"
      "\xDC\x3F\x3D\x1F\xD4\xE5\x5E\x1C" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17"
    , ""
    , "\x28\x20\x26\xDA\x30\x68\xBC\x9F\xA1\x18\x68\x1D\x55\x9F\x10\xF6");

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xFC\xFC\xEE\x7A\x2A\x8D\x4D\x48\x6E\xF2\xF5\x25\x87\xFD\xA0\xED"
      "\x97\xDC\x7E\xED\xE2\x41\xDF\x68" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xCE\xAA\xB9\xB0\x5D\xF7\x71\xA6\x57\x14\x9D\x53\x77\x34\x63\xCB"
      "\xB2\xA0\x40\xDD\x3B\xD5\x16\x43\x72\xD7\x6D\x7B\xB6\x82\x42\x40" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
    , ""
    , "\xE1\xE0\x72\x63\x3B\xAD\xE5\x1A\x60\xE8\x59\x51\xD9\xC4\x2A\x1B" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xCE\xAA\xB9\xB0\x5D\xF7\x71\xA6\x57\x14\x9D\x53\x77\x34\x63\xCB"
      "\x4A\x3B\xAE\x82\x44\x65\xCF\xDA\xF8\xC4\x1F\xC5\x0C\x7D\xF9\xD9" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
      "\x20\x21\x22\x23\x24\x25\x26\x27"
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
      "\x20\x21\x22\x23\x24\x25\x26\x27"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xCE\xAA\xB9\xB0\x5D\xF7\x71\xA6\x57\x14\x9D\x53\x77\x34\x63\xCB"
      "\x68\xC6\x57\x78\xB0\x58\xA6\x35\x65\x9C\x62\x32\x11\xDE\xEA\x0D"
      "\xE3\x0D\x2C\x38\x18\x79\xF4\xC8" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
      "\x20\x21\x22\x23\x24\x25\x26\x27"
    , ""
    , "\x7A\xEB\x7A\x69\xA1\x68\x7D\xD0\x82\xCA\x27\xB0\xD9\xA3\x70\x96" );

  TEST_VECTOR( ietf_key, ietf_nonce
    , ""
    , "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
      "\x20\x21\x22\x23\x24\x25\x26\x27"
    , "\xBE\xA5\xE8\x79\x8D\xBE\x71\x10\x03\x1C\x14\x4D\xA0\xB2\x61\x22"
      "\xCE\xAA\xB9\xB0\x5D\xF7\x71\xA6\x57\x14\x9D\x53\x77\x34\x63\xCB"
      "\x68\xC6\x57\x78\xB0\x58\xA6\x35\x06\x0C\x84\x67\xF4\xAB\xAB\x5E"
      "\x8B\x3C\x20\x67\xA2\xE1\x15\xDC" );


  /* Some big texts.  These were originally encrypted using this program;
     they are regression tests. */

  TEST_VECTOR(
      "\x06\xF8\x9F\x69\xDA\x49\xDA\xD7\x68\x48\xFF\xB3\x60\xB6\x8F\x00"
    , "\xDC\xBF\x85\x18\x23\xD9\x67\x85\x45\x59\x6F\xAD"
    , ""
    , "\xC4\x8E\x1F\x04\x10\x2F\xA5\x58\x68\x42\x62\xF3\x1B\xE7\x63\xA7"
      "\x77\x89\x64\x16\xE6\xB0\xF7\xFA\xFE\xF0\xB9\x50\x22\xDC\xCE\x78"
      "\xA5\x01\xA4\x2D\xA2\x0F\x50\xEA\x9A\xAE\x23\x60\x1C\xC9\x11\x84"
      "\x5F\xD0\x0A\x88\x99\xCD\xF1\x1B\x7C\xF9\x71\xC2\xD8\xE3\x7B\xB1"
      "\xBC\x91\xCE\x10\xDA\xED\xBF\xA5\xDF\x96\x69\x7A\xFA\xCA\x9A\x91"
      "\x40\xF9\x8F\xF1\xFE\x5A\x2B\x21\x17\xA5\xAE\x97\xBD\x0B\x68\xE0"
      "\x58\x7A\xB4\x10\x89\xDE\x30\x12\xB7\x14\xEB\xE8\xFA\xA5\x87\x7A"
      "\x7E\xEA\xD0\x34\x0B\xAF\x73\x0B\x1C\xA7\xC5\xCB\x84\x15\x5F\x45"
      "\x53\x8E\x27\xD7\xA0\x7D\xAF\x99\x05\x6E\x9F\xF4\xCD\xE5\xA1\x11"
      "\x43\xBD\x7A\x24\xA3\xB7\xB6\x92\x7C\xB7\xD8\x6F\x17\x78\xB3\x30"
      "\x53\x6E\x46\x14\x11\x86\xF6\x70\x2E\x25\x8E\x54\xF8\x70\x2F\x47"
      "\x2B\xF8\xF6\xC3\xEE\xFB\x9B\x7D\xF0\x73\xAD\x24\xC0\xE9\x58\xA8"
      "\x79\xDC\x92\x25\x6D\xF3\x9D\xA8\x8F\x07\xA3\xE7\xAB\xF4\xBC\x32"
      "\x29\xB6\xEF\xDB\x20\x42\x51\xCF\xD5\x57\xDA\x14\xC7\x2A\xB3\x19"
      "\xDE\x31\xF7\x77\x95\x8D\x33\x57\x1D\xEB\x9A\x80\x98\x4D\x95\x07"
      "\xEB\x38\x42\xEB\xE0\xAB\x2D\xF3\x9E\x16\xFA\x6D\x06\x62\x41\x58"
      "\x81\xBF\x3D\x01\x1A\xF7\xBA\x53\xCE\x60\xEF\x86\xC9\x11\xED\x89"
      "\x5B\xCF\x98\xB4\xD3\xD4\xAC\xB9\x71\x69\xEE\xF6\xA7\x77\x4E\x46"
      "\x0F\xF7\x1D\x7D\x36\x4E\x86\x15\xCD\xF3\xD3\x5C\xAA\x97\x74\xCC"
      "\x5D\xD6\xF0\x5C\x62\x27\x0D\x4B\x01\xDD\x8B\xE6\x2C\x72\x99\xBA"
      "\x5B\x95\x87\x0F\xB9\x9A\xBE\xA2\x42\x5E\x7D\x1B\x19\x08\xBC\xEF"
      "\x66\x24\x2F\xB3\xFC\xE7\xDA\xB1\x79\x51\xE9\x3B\x45\x66\xDC\xD8"
      "\x0F\xA7\x56\xBF\x56\x15\x4B\x15\xCC\x3E\x9F\x44\xB9\xED\xC2\x0A"
      "\x7C\xA9\x6D\x87\xCD\xE2\x00\x9F\x2F\x2C\xF0\xBC\x1B\xB0\x27\xE8"
      "\x46\x3E\x06\x1E\xD5\xB6\xB4\x82\xF7\x88\xB7\x48\xF3\x58\xB0\x23"
      "\x6D\xC2\xD3\x85\x9D\x1F\xE5\x30\xFC\xC8\x46\x62\xD1\xE1\xAE\x3B"
      "\x3A\xA9\x57\xE0\xD7\x8F\x7E\x4D\x59\x7D\x7A\xEB\xBF\xD3\x10\x00"
      "\xA0\xE4\x76\x76\xE3\xA8\x14\xD0\x03\xBF\x1F\x6A\xF9\x11\xCE\x98"
      "\x2C\x2A\x86\x25\x77\x85\x14\x76\xD4\x51\xAB\xC7\x3A\xA7\xE1\xF7"
      "\x23\xF7\x2B\xA3\xBA\xE4\x0B\xA4\x81\x9A\x83\x98\x69\xC3\x1C\x8A"
      "\xBD\x26\x12\x36\x22\x9D\xCE\x85\x5D\xA3\xA0\xDF\x66\xD0\x59\xF6"
      "\x47\xF2\xC5\x37\xF1\x62\x0D\x0C\x45\x5B\xE5\xFE\x3C\x8D\x28\x75"
    , "\xa1\xd8\xa0\xe0\x75\x5c\xb4\xf4\xab\x59\x6d\x14\xfc\x2e\x75\x54"
      "\xa3\x35\x4f\x57\x69\x48\x7a\x46\x17\x5f\xd9\x34\x50\xf9\x35\xe5"
      "\x6f\xee\x27\xdb\x28\x0f\x06\x0b\xaf\xd5\x50\x4e\x20\x78\x35\xd6"
      "\x4d\xa0\x18\xe8\x6c\x5b\x07\xbb\xb6\xd0\x3f\x4a\x0e\x14\x32\xaf"
      "\x0d\x6a\x5d\xb6\xe4\x36\xb0\x1c\xae\x2e\x75\x85\x19\x53\x9d\xf2"
      "\xc6\xc6\xf5\x29\x4a\x5d\x73\xd6\xcd\x3a\xec\x38\x15\x7b\x1f\x3d"
      "\x19\x8f\x7f\x85\x53\x75\xfe\x6c\x8a\x5d\x4f\x05\x3f\x16\x5e\x73"
      "\x9c\xbe\xbe\xdb\xe8\xb2\x3e\xea\x0f\x2e\x2c\xf3\x2c\x9c\x56\x5b"
      "\xf7\xf2\xed\x87\x07\xfb\x87\x74\x09\xda\xb8\xad\x57\xc0\xa0\x79"
      "\xc6\x69\x1c\x79\x08\x69\x35\x18\xc4\x54\xca\x6b\xed\x89\xa0\x27"
      "\x32\x19\x1f\xaf\xff\x12\x1c\x1b\x08\xa6\x9f\x0e\xe3\x01\x98\x77"
      "\xdb\x75\xdf\x87\x67\x6d\xd9\xb4\x23\x39\xd0\x81\x54\xf8\x89\x18"
      "\x5c\xbd\xb7\x4d\xf4\xb1\x8b\x37\x2e\x9b\x20\x29\x9f\x95\x9e\xdd"
      "\x73\x2c\xdb\x37\xe5\x7d\x9f\x8a\x79\xff\x8f\x5b\x99\x10\xe1\xe9"
      "\x1d\xcb\x1f\x17\xef\x7d\xb7\xd1\x50\x23\x4a\x38\x92\xfb\xbe\x1c"
      "\xff\x23\x68\x3a\xb9\xc8\xbc\xa0\xab\xa7\xbe\xc8\x84\xce\x66\xa7"
      "\x11\x17\xc6\x48\x91\x4d\x77\xe1\x64\xc4\x26\x08\xcd\xb4\xea\x10"
      "\x42\x60\x5a\x7c\x5a\x72\xba\xb4\x93\xf7\x02\xa1\xd4\x44\xdb\x1a"
      "\x4a\xc3\xb1\xea\x69\x74\xea\x18\xb3\x5a\x27\x09\x6f\x5b\x1f\x30"
      "\xe2\xeb\x2a\x37\x5e\xd2\xe2\x23\xec\xbc\xcb\xcb\x65\x41\xed\x0e"
      "\x23\xda\x71\x45\xf3\x3f\x7d\x44\x73\xd7\x14\x4d\xab\x03\xf7\x7d"
      "\x24\x33\x9d\x25\x83\x1f\x3d\xf3\xc9\x34\x42\x6c\x2f\x61\xa2\x83"
      "\xa0\x0f\xb6\x28\xe8\x27\xf0\x91\xf6\xc5\x7e\xa1\x74\x74\xaf\xa7"
      "\x7f\xba\x99\x29\xb2\x91\x9f\xf0\xf7\x71\x1f\xf0\x0b\xd6\xd9\xf9"
      "\x72\xaa\x04\x09\x5d\x75\x76\x5f\x18\xc8\xd5\xd5\x84\x1a\x40\x3f"
      "\xe5\x38\xe4\x38\xdf\xf9\x0d\x79\xdc\x71\xc8\xa8\xef\x86\x4e\x26"
      "\xf4\xc2\x46\xf4\xe0\x64\xd6\x5e\xfb\x1c\x47\x58\x3d\x87\x7c\xba"
      "\xa3\xf5\x98\xdc\xd5\xdf\xaf\x62\x96\xee\x4e\x39\x32\x4c\x6d\xd2"
      "\x0a\x6f\xf8\x34\x98\x76\xae\x21\xa1\x41\x3d\x96\xfc\x52\xdb\xdd"
      "\xf3\x9a\xb1\xf3\x78\x3d\xb8\x2f\xae\xae\x7d\xe0\x4b\xb2\xdf\x2b"
      "\x12\xac\xee\xfb\xf8\x54\x60\xea\x74\xd5\xde\x43\x7b\x38\x45\x5f"
      "\xa0\xb8\xe8\xcc\x8a\xe3\xcc\x0c\x92\xe6\xb1\xb0\xe2\xc1\x99\xca"
      "\x1b\xa1\xac\x6e\x7a\x8a\xa0\x20\x3d\xeb\x29\x8b\xf4\x55\x41\x62" );

  TEST_VECTOR(
      "\x7A\x54\x0D\x3E\x56\x38\xF7\xC6\xCF\xAB\xF9\x56\xDC\xCA\x14\x23"
    , "\x9B\x0E\xC1\x15\xD5\xE6\xC9\xAB\xE6\x88\x2A\x18"
    , ""
    , "\x52\xDB\xA7\x44\x2B\x1C\x9C\x24\x4D\xF3\xA1\xE4\x53\x7B\x9B\xB2"
      "\x25\xC5\xA3\x81\x42\x23\xA9\xB4\x12\xF8\xFC\xE4\xF6\x8E\x20\xD4"
      "\x59\x7B\x39\x2D\x5D\x7C\x6E\xB7\x51\x02\x90\x7A\x8E\xAA\x30\xD0"
      "\xEB\xDF\x70\x09\x5A\xEC\xFB\xD4\xDB\x0B\xE9\x1B\x79\xAF\x40\xA3"
      "\xC9\xD1\x3F\x0E\x7E\x9B\xA8\xF2\x4E\xA2\xAE\x81\xE7\x8F\xE9\x4A"
      "\xF8\x84\x38\x20\x9B\xCB\x0D\x9E\x46\x87\xCC\x0C\x71\xF2\x4C\xE5"
      "\x57\x47\xFB\x52\x1D\x0C\x92\xCD\x9B\xD0\xA3\xA3\xFE\x44\x4F\x2A"
      "\x2D\x7D\xC8\x48\x87\x71\x8A\x83\x83\x43\x52\xD9\x87\x72\x0A\x1F"
      "\x72\x7D\xE0\x0D\xE4\xBA\x38\x2D\xAB\x89\x85\x34\xFE\x30\xE9\x47"
      "\x99\x69\x96\x37\x3B\x88\x0D\x08\x5B\x41\xF0\x55\x3A\x90\xB6\x5D"
      "\xC9\xB8\xFA\x22\xEB\x54\x67\x1B\x43\x3B\x2A\x13\xDF\x76\x71\xBF"
      "\x8F\xBA\xF6\x5F\xE0\xAE\x84\x80\xD3\x9A\x9E\xFE\x35\x29\x0C\x44"
      "\xB8\x3B\xC8\x58\x3F\x51\x18\xF0\x8E\x3F\xE0\xFE\xFF\x55\xE1\xFD"
      "\x79\xED\x71\x6D\xB9\xBE\xF0\x76\x25\x4E\x6F\xA2\x49\x5B\xEC\x20"
      "\x61\x95\x65\x67\xBF\xA3\x73\x18\x65\x7C\x2F\x94\x29\x3B\x57\x7D"
      "\x78\x6A\xBD\x22\x2B\x17\x5E\xDA\x2B\x4E\xC3\xA5\xF0\x9A\xD9\x1C"
      "\xCF\xC4\xDD\x7E\x07\xB4\xE6\x50\xD0\x9D\xAF\x86\x9D\x3F\xFE\x70"
      "\x64\x18\xBA\x32\x90\xE5\x63\xA0\x08\x3D\x1D\xCE\xB2\x77\x31\x59"
      "\xD4\xB4\x03\x2C\xE2\x15\x11\x17\xD1\xBA\x5E\x1E\x7D\x38\x09\xCF"
      "\x25\x3E\x3D\x89\x7E\x13\xAE\x11\xFA\xCD\x46\x23\x86\xC5\x78\xC8"
      "\x1A\xE8\x8F\x9A\x9C\xCE\xBE\x01\x5D\x70\x56\xA8\x84\x07\x07\x2A"
      "\x1C\xD4\x33\x8B\x2F\x01\x0F\xD0\xCC\xCD\x29\xEC\x73\x6E\xDE\xB2"
      "\xBC\xA7\x32\x6A\x12\x93\x13\xCD\xE0\xA9\x55\x97\x36\x6B\xDA\xC5"
      "\xC5\xD2\x44\x80\xBD\x00\x37\xD7\x21\x80\x27\x1F\x2E\x02\x76\x1A"
      "\x57\xA5\x5F\xCE\x8B\x20\x6A\x6A\x0F\x70\x9B\xDB\x07\x64\x96\x7B"
      "\x43\xFD\xCA\xBF\x9D\x52\xEB\x24\x7B\x1B\xEE\x43\x10\x2C\xDB\x92"
      "\xA1\xA8\xA9\xF7\x2F\xD2\x39\xA8\x85\x9C\xFE\x2C\x2A\xCF\x52\x73"
      "\xFB\xCA\x20\xAD\xC9\xDD\xFC\x4A\x91\x39\x6C\x7C\x84\x67\xC5\xE4"
      "\x9B\x3E\x3D\x6B\x56\x3B\x2B\xDC\x8A\x46\xF6\x7C\x36\xF9\x27\x29"
      "\x37\x38\x7C\x9D\xA0\x6E\x5D\x4C\xE5\xB2\x6F\x0C\xDC\xEF\xFE\x35"
      "\xFE\x3D\x56\x40\x7F\xBD\x4D\xDD\x40\x79\xDD\xA7\x0A\x7B\xA2\xCE"
      "\x22\x38\x94\xEA\x90\xF5\x95\xB6\xE6\x6F\x14\xFB\xA2"
    , "\xec\xa9\xcc\x30\x66\x6c\x04\x16\x21\x8d\xc8\x15\x47\xa2\x18\xcf"
      "\x19\x90\x4f\x82\x27\x25\xa2\x1e\xfa\x1c\xe4\x58\x78\x43\x52\x4c"
      "\xac\x24\xde\xcb\xad\x80\x05\x7a\xeb\x2d\xc0\x33\x05\x31\x25\x44"
      "\xd7\x11\xa1\xf2\xcb\x09\x6f\xf0\x14\x3c\x3f\xf2\xc7\x79\xfb\x3f"
      "\xb0\x0a\x65\xae\xd7\xe5\x5d\x35\x0c\xb4\x69\x7a\x89\x6b\xa9\xdc"
      "\x02\x69\x96\xd2\x9f\xe7\x3c\x99\xd4\xd3\x55\x97\xc4\x59\xad\xc4"
      "\x0c\x7b\xf8\x47\x1c\xbe\x36\x2a\x53\x6b\xb1\x21\x5f\xc1\x6e\xca"
      "\x0a\x4f\x16\x3a\xf0\xd4\x12\xa6\xf8\x68\x9f\x12\xad\x36\x4c\xd8"
      "\x5a\x5b\x17\xb8\xbd\xc7\x2c\x48\x51\x7d\x45\x74\x00\xb0\x02\xe9"
      "\x1b\xd6\x0c\x41\xa7\x5d\x65\xca\x68\xa7\x3e\x3c\xf5\xaa\x9b\xbd"
      "\x25\x98\x00\xd8\x4d\xbc\xd1\x7a\x25\x34\x92\x24\xa4\x84\x62\x63"
      "\x2c\x40\xa5\x58\x81\x90\xf1\x0f\x75\xaa\x70\xe4\x4e\x0f\xa3\x03"
      "\x90\xd1\x07\x18\x0a\x50\x9a\x3e\x28\x1f\x33\xb7\x11\xed\x3c\x2c"
      "\x40\xc8\xd7\xe3\x12\xdc\xef\x94\x93\x7b\x11\xc3\x24\x51\x61\xbf"
      "\x8b\xa4\xa4\x5c\x85\x0d\x50\x49\x45\x69\xbe\x5b\x36\x90\x84\x30"
      "\x66\x67\x76\x3d\xcc\x02\x8b\x9f\xb7\x90\x57\xef\xe1\x21\x34\x65"
      "\x3e\xca\xbf\x70\x1d\x76\x63\xbf\xae\x1f\xb2\x55\xf0\x87\x3e\x42"
      "\xf9\x71\x28\x02\x06\x9e\xf7\x6a\x47\x3b\xda\x38\x54\x66\xd9\xaf"
      "\xba\x7b\xec\xbf\xe3\x52\x63\x02\x8b\xa7\xad\x1d\x76\x16\xa2\x20"
      "\x38\xec\x40\xb7\xc8\x35\x6b\xc2\x80\x9d\x20\x02\xc6\x34\xdb\x65"
      "\xd8\x27\x0b\xc5\x2d\x85\xe4\xdc\x85\xae\x10\x36\x01\xdb\x4b\xaf"
      "\x44\x79\xea\x23\x21\xa0\x83\xa3\x91\xf5\xc5\x16\x9b\xeb\x43\x92"
      "\x1f\x88\xd2\x00\x60\x40\xe9\x52\x0b\x39\x86\x3b\x9e\x3b\x9a\x4a"
      "\x31\xdf\xb6\x57\x78\x38\xcf\x77\x7c\x0c\xf4\x14\x90\x25\xed\x27"
      "\xd2\x86\x20\x4c\x1a\x52\xeb\xbe\x1e\xac\x2b\xce\xb7\x72\x86\x87"
      "\xfd\xac\x11\x90\xc5\xea\x96\xcb\xdc\x89\xe9\x77\xf0\x83\xc3\xa7"
      "\xa7\xd1\xe1\xc9\x7e\x89\xb3\x4e\xf1\x12\xa3\x9c\xfe\x66\xcc\x5d"
      "\xcf\x1d\x5a\x11\x21\x2f\x10\x66\x37\x5f\xd7\x35\xeb\x09\x62\x99"
      "\xa6\xf8\xc7\xc7\xef\xd3\xf0\x56\x2b\xa7\x14\x65\x6a\xce\xa9\x68"
      "\xe7\xa4\x89\xb4\x1e\x16\x99\xbf\x8d\x2d\x5e\x67\xb4\x3a\x0b\xf3"
      "\x37\x14\x1e\x5d\xc6\xb4\xb5\x9e\xa5\x69\xa4\xaf\xcc\x0f\x46\xe9"
      "\xd5\xbb\x10\x49\x07\x0d\x92\x42\x0c\x04\xb9\xdf\xa4\xb5\xef\xcc"
      "\x05\x81\x3f\xc1\x21\x12\x2c\x33\xb7\x79\xfd\x5d\x8a" );
}

/* http://tools.ietf.org/html/draft-krovetz-ocb-03#appendix-A
   also specifies an iterative test algorithm, which we implement here. */

static void test_iterative( void ) {
  /* Key is always all zeros */
  AlignedBuffer key( KEY_LEN );
  memset( key.data(), 0, KEY_LEN );

  AlignedBuffer *ctx_buf = get_ctx( key );
  ae_ctx *ctx = (ae_ctx *)ctx_buf->data();

  AlignedBuffer nonce( NONCE_LEN );
  memset( nonce.data(), 0, NONCE_LEN );

  /* The loop below fills this buffer in order.
     Each iteration adds 2*i + 3*TAG_LEN bytes. */
  AlignedBuffer accumulator( 22400 );
  uint8_t *acc = (uint8_t *) accumulator.data();

  for ( size_t i=0; i<128; i++ ) {
    /* i bytes of zeros */
    AlignedBuffer s( i );
    memset( s.data(), 0, s.len() );

    /* Nonce is 11 bytes of zeros followed by 1 byte i */
    ( (uint8_t *) nonce.data() )[ 11 ] = i;

    /* We can't write directly to acc because it might not be aligned. */
    AlignedBuffer out( s.len() + TAG_LEN );

    /* OCB-ENCRYPT(K,N,S,S) */
    fatal_assert( 0 <= ae_encrypt( ctx, nonce.data(),
                                   s.data(), s.len(),
                                   s.data(), s.len(),
                                   out.data(), NULL,
                                   AE_FINALIZE ) );
    memcpy( acc, out.data(), s.len() + TAG_LEN );
    acc += s.len() + TAG_LEN;

    /* OCB-ENCRYPT(K,N,<empty string>,S) */
    fatal_assert( 0 <= ae_encrypt( ctx, nonce.data(),
                                   s.data(), s.len(),
                                   NULL, 0,
                                   out.data(), NULL,
                                   AE_FINALIZE ) );
    memcpy( acc, out.data(), s.len() + TAG_LEN );
    acc += s.len() + TAG_LEN;

    /* OCB-ENCRYPT(K,N,S,<empty string>) */
    fatal_assert( 0 <= ae_encrypt( ctx, nonce.data(),
                                   NULL, 0,
                                   s.data(), s.len(),
                                   out.data(), NULL,
                                   AE_FINALIZE ) );
    memcpy( acc, out.data(), TAG_LEN );
    acc += TAG_LEN;
  }

  /* OCB-ENCRYPT(K,N,C,<empty string>) */
  AlignedBuffer out( TAG_LEN );
  memset( nonce.data(), 0, NONCE_LEN );
  fatal_assert( 0 <= ae_encrypt( ctx, nonce.data(),
                                 NULL, 0,
                                 accumulator.data(), accumulator.len(),
                                 out.data(), NULL,
                                 AE_FINALIZE ) );

  /* Check this final tag against the known value */
  AlignedBuffer correct( TAG_LEN, "\xB2\xB4\x1C\xBF\x9B\x05\x03\x7D\xA7\xF1\x6C\x24\xA3\x5C\x1C\x94" );
  fatal_assert( equal( out, correct ) );

  scrap_ctx( ctx_buf );

  if ( verbose ) {
    printf( "iterative PASSED\n\n" );
  }
}

int main( int argc, char *argv[] )
{
  if ( argc >= 2 && strcmp( argv[ 1 ], "-v" ) == 0 ) {
    verbose = true;
  }

  test_all_vectors();
  test_iterative();

  return 0;
}
