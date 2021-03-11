/* hmac.c
 *
 * Copyright (C) 2006-2021 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

#ifndef NO_HMAC

#if defined(HAVE_FIPS) && \
    defined(HAVE_FIPS_VERSION) && (HAVE_FIPS_VERSION >= 2)

    /* set NO_WRAPPERS before headers, use direct internal f()s not wrappers */
    #define FIPS_NO_WRAPPERS

    #ifdef USE_WINDOWS_API
        #pragma code_seg(".fipsA$b")
        #pragma const_seg(".fipsB$b")
    #endif
#endif

#include <wolfssl/wolfcrypt/hmac.h>

#ifdef WOLF_CRYPTO_CB
    #include <wolfssl/wolfcrypt/cryptocb.h>
#endif

#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif

#ifdef WOLFSSL_KCAPI_HMAC
    #include <wolfssl/wolfcrypt/port/kcapi/kcapi_hmac.h>

    #define wc_HmacSetKey  wc_HmacSetKey_Software
    #define wc_HmacUpdate  wc_HmacUpdate_Software
    #define wc_HmacFinal   wc_HmacFinal_Software
#endif


/* fips wrapper calls, user can call direct */
/* If building for old FIPS. */
#if defined(HAVE_FIPS) && \
    (!defined(HAVE_FIPS_VERSION) || (HAVE_FIPS_VERSION < 2))

    /* does init */
    int wc_HmacSetKey(Hmac* hmac, int type, const byte* key, word32 keySz)
    {
        if (hmac == NULL || (key == NULL && keySz != 0) ||
           !(type == WC_MD5 || type == WC_SHA || type == WC_SHA256 ||
                type == WC_SHA384 || type == WC_SHA512)) {
            return BAD_FUNC_ARG;
        }

        return HmacSetKey_fips(hmac, type, key, keySz);
    }
    int wc_HmacUpdate(Hmac* hmac, const byte* in, word32 sz)
    {
        if (hmac == NULL || (in == NULL && sz > 0)) {
            return BAD_FUNC_ARG;
        }

        return HmacUpdate_fips(hmac, in, sz);
    }
    int wc_HmacFinal(Hmac* hmac, byte* out)
    {
        if (hmac == NULL) {
            return BAD_FUNC_ARG;
        }

        return HmacFinal_fips(hmac, out);
    }
    int wolfSSL_GetHmacMaxSize(void)
    {
        return CyaSSL_GetHmacMaxSize();
    }

    int wc_HmacInit(Hmac* hmac, void* heap, int devId)
    {
    #ifndef WOLFSSL_KCAPI_HMAC
        (void)hmac;
        (void)heap;
        (void)devId;
        return 0;
    #else
        return HmacInit(hmac, heap, devId);
    #endif
    }
    void wc_HmacFree(Hmac* hmac)
    {
    #ifndef WOLFSSL_KCAPI_HMAC
        (void)hmac;
    #else
        HmacFree(hmac);
    #endif
    }

    #ifdef HAVE_HKDF
        int wc_HKDF(int type, const byte* inKey, word32 inKeySz,
                    const byte* salt, word32 saltSz,
                    const byte* info, word32 infoSz,
                    byte* out, word32 outSz)
        {
            return HKDF(type, inKey, inKeySz, salt, saltSz,
                info, infoSz, out, outSz);
        }
    #endif /* HAVE_HKDF */

#else /* else build without fips, or for new fips */


int wc_HmacSizeByType(int type)
{
    int ret;

    if (!(type == WC_MD5 || type == WC_SHA ||
            type == WC_SHA224 || type == WC_SHA256 ||
            type == WC_SHA384 || type == WC_SHA512 ||
            type == WC_SHA3_224 || type == WC_SHA3_256 ||
            type == WC_SHA3_384 || type == WC_SHA3_512)) {
        return BAD_FUNC_ARG;
    }

    switch (type) {
    #ifndef NO_MD5
        case WC_MD5:
            ret = WC_MD5_DIGEST_SIZE;
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            ret = WC_SHA_DIGEST_SIZE;
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            ret = WC_SHA224_DIGEST_SIZE;
            break;
    #endif /* WOLFSSL_SHA224 */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = WC_SHA256_DIGEST_SIZE;
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = WC_SHA384_DIGEST_SIZE;
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = WC_SHA512_DIGEST_SIZE;
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
        case WC_SHA3_224:
            ret = WC_SHA3_224_DIGEST_SIZE;
            break;

        case WC_SHA3_256:
            ret = WC_SHA3_256_DIGEST_SIZE;
            break;

        case WC_SHA3_384:
            ret = WC_SHA3_384_DIGEST_SIZE;
            break;

        case WC_SHA3_512:
            ret = WC_SHA3_512_DIGEST_SIZE;
            break;

    #endif

        default:
            ret = BAD_FUNC_ARG;
            break;
    }

    return ret;
}

int _InitHmac(Hmac* hmac, int type, void* heap)
{
    int ret = 0;
#ifdef WOLF_CRYPTO_CB
    int devId = hmac->devId;
#else
    int devId = INVALID_DEVID;
#endif
    switch (type) {
    #ifndef NO_MD5
        case WC_MD5:
            ret = wc_InitMd5_ex(&hmac->hash.md5, heap, devId);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_InitSha_ex(&hmac->hash.sha, heap, devId);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            ret = wc_InitSha224_ex(&hmac->hash.sha224, heap, devId);
            break;
    #endif /* WOLFSSL_SHA224 */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_InitSha256_ex(&hmac->hash.sha256, heap, devId);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_InitSha384_ex(&hmac->hash.sha384, heap, devId);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_InitSha512_ex(&hmac->hash.sha512, heap, devId);
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            ret = wc_InitSha3_224(&hmac->hash.sha3, heap, devId);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            ret = wc_InitSha3_256(&hmac->hash.sha3, heap, devId);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            ret = wc_InitSha3_384(&hmac->hash.sha3, heap, devId);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            ret = wc_InitSha3_512(&hmac->hash.sha3, heap, devId);
            break;
    #endif
    #endif

        default:
            ret = BAD_FUNC_ARG;
            break;
    }

    /* default to NULL heap hint or test value */
#ifdef WOLFSSL_HEAP_TEST
    hmac->heap = (void*)WOLFSSL_HEAP_TEST;
#else
    hmac->heap = heap;
#endif /* WOLFSSL_HEAP_TEST */

    return ret;
}


int wc_HmacSetKey(Hmac* hmac, int type, const byte* key, word32 length)
{
    byte*  ip;
    byte*  op;
    word32 i, hmac_block_size = 0;
    int    ret = 0;
    void*  heap = NULL;

    if (hmac == NULL || (key == NULL && length != 0) ||
       !(type == WC_MD5 || type == WC_SHA ||
            type == WC_SHA224 || type == WC_SHA256 ||
            type == WC_SHA384 || type == WC_SHA512 ||
            type == WC_SHA3_224 || type == WC_SHA3_256 ||
            type == WC_SHA3_384 || type == WC_SHA3_512)) {
        return BAD_FUNC_ARG;
    }

#ifndef HAVE_FIPS
    /* if set key has already been run then make sure and free existing */
    /* This is for async and PIC32MZ situations, and just normally OK,
       provided the user calls wc_HmacInit() first. That function is not
       available in FIPS builds. In current FIPS builds, the hashes are
       not allocating resources. */
    if (hmac->macType != WC_HASH_TYPE_NONE) {
        wc_HmacFree(hmac);
    }
#endif

    hmac->innerHashKeyed = 0;
    hmac->macType = (byte)type;

    ret = _InitHmac(hmac, type, heap);
    if (ret != 0)
        return ret;

#ifdef HAVE_FIPS
    if (length < HMAC_FIPS_MIN_KEY)
        return HMAC_MIN_KEYLEN_E;
#endif

#ifdef WOLF_CRYPTO_CB
    hmac->keyRaw = key; /* use buffer directly */
    hmac->keyLen = length;
#endif

    ip = (byte*)hmac->ipad;
    op = (byte*)hmac->opad;

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            hmac_block_size = WC_MD5_BLOCK_SIZE;
            if (length <= WC_MD5_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Md5Update(&hmac->hash.md5, key, length);
                if (ret != 0)
                    break;
                ret = wc_Md5Final(&hmac->hash.md5, ip);
                if (ret != 0)
                    break;
                length = WC_MD5_DIGEST_SIZE;
            }
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            hmac_block_size = WC_SHA_BLOCK_SIZE;
            if (length <= WC_SHA_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_ShaUpdate(&hmac->hash.sha, key, length);
                if (ret != 0)
                    break;
                ret = wc_ShaFinal(&hmac->hash.sha, ip);
                if (ret != 0)
                    break;

                length = WC_SHA_DIGEST_SIZE;
            }
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            hmac_block_size = WC_SHA224_BLOCK_SIZE;
            if (length <= WC_SHA224_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha224Update(&hmac->hash.sha224, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha224Final(&hmac->hash.sha224, ip);
                if (ret != 0)
                    break;

                length = WC_SHA224_DIGEST_SIZE;
            }
            break;
    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
        case WC_SHA256:
            hmac_block_size = WC_SHA256_BLOCK_SIZE;
            if (length <= WC_SHA256_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha256Update(&hmac->hash.sha256, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha256Final(&hmac->hash.sha256, ip);
                if (ret != 0)
                    break;

                length = WC_SHA256_DIGEST_SIZE;
            }
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            hmac_block_size = WC_SHA384_BLOCK_SIZE;
            if (length <= WC_SHA384_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha384Update(&hmac->hash.sha384, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha384Final(&hmac->hash.sha384, ip);
                if (ret != 0)
                    break;

                length = WC_SHA384_DIGEST_SIZE;
            }
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            hmac_block_size = WC_SHA512_BLOCK_SIZE;
            if (length <= WC_SHA512_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha512Update(&hmac->hash.sha512, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha512Final(&hmac->hash.sha512, ip);
                if (ret != 0)
                    break;

                length = WC_SHA512_DIGEST_SIZE;
            }
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            hmac_block_size = WC_SHA3_224_BLOCK_SIZE;
            if (length <= WC_SHA3_224_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha3_224_Update(&hmac->hash.sha3, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha3_224_Final(&hmac->hash.sha3, ip);
                if (ret != 0)
                    break;

                length = WC_SHA3_224_DIGEST_SIZE;
            }
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            hmac_block_size = WC_SHA3_256_BLOCK_SIZE;
            if (length <= WC_SHA3_256_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha3_256_Update(&hmac->hash.sha3, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha3_256_Final(&hmac->hash.sha3, ip);
                if (ret != 0)
                    break;

                length = WC_SHA3_256_DIGEST_SIZE;
            }
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            hmac_block_size = WC_SHA3_384_BLOCK_SIZE;
            if (length <= WC_SHA3_384_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha3_384_Update(&hmac->hash.sha3, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha3_384_Final(&hmac->hash.sha3, ip);
                if (ret != 0)
                    break;

                length = WC_SHA3_384_DIGEST_SIZE;
            }
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            hmac_block_size = WC_SHA3_512_BLOCK_SIZE;
            if (length <= WC_SHA3_512_BLOCK_SIZE) {
                if (key != NULL) {
                    XMEMCPY(ip, key, length);
                }
            }
            else {
                ret = wc_Sha3_512_Update(&hmac->hash.sha3, key, length);
                if (ret != 0)
                    break;
                ret = wc_Sha3_512_Final(&hmac->hash.sha3, ip);
                if (ret != 0)
                    break;

                length = WC_SHA3_512_DIGEST_SIZE;
            }
            break;
    #endif
    #endif /* WOLFSSL_SHA3 */

        default:
            return BAD_FUNC_ARG;
    }

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_HMAC)
    if (hmac->asyncDev.marker == WOLFSSL_ASYNC_MARKER_HMAC) {
    #if defined(HAVE_INTEL_QA) || defined(HAVE_CAVIUM)
        #ifdef HAVE_INTEL_QA
        if (IntelQaHmacGetType(hmac->macType, NULL) == 0)
        #endif
        {
            if (length > hmac_block_size)
                length = hmac_block_size;
            /* update key length */
            hmac->keyLen = (word16)length;

            return ret;
        }
        /* no need to pad below */
    #endif
    }
#endif

    if (ret == 0) {
        if (length < hmac_block_size)
            XMEMSET(ip + length, 0, hmac_block_size - length);

        for(i = 0; i < hmac_block_size; i++) {
            op[i] = ip[i] ^ OPAD;
            ip[i] ^= IPAD;
        }
    }

    return ret;
}


static int HmacKeyInnerHash(Hmac* hmac)
{
    int ret = 0;

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            ret = wc_Md5Update(&hmac->hash.md5, (byte*)hmac->ipad,
                                                             WC_MD5_BLOCK_SIZE);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_ShaUpdate(&hmac->hash.sha, (byte*)hmac->ipad,
                                                             WC_SHA_BLOCK_SIZE);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            ret = wc_Sha224Update(&hmac->hash.sha224, (byte*)hmac->ipad,
                                                          WC_SHA224_BLOCK_SIZE);
            break;
    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_Sha256Update(&hmac->hash.sha256, (byte*)hmac->ipad,
                                                          WC_SHA256_BLOCK_SIZE);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_Sha384Update(&hmac->hash.sha384, (byte*)hmac->ipad,
                                                          WC_SHA384_BLOCK_SIZE);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_Sha512Update(&hmac->hash.sha512, (byte*)hmac->ipad,
                                                          WC_SHA512_BLOCK_SIZE);
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            ret = wc_Sha3_224_Update(&hmac->hash.sha3, (byte*)hmac->ipad,
                                                        WC_SHA3_224_BLOCK_SIZE);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            ret = wc_Sha3_256_Update(&hmac->hash.sha3, (byte*)hmac->ipad,
                                                        WC_SHA3_256_BLOCK_SIZE);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            ret = wc_Sha3_384_Update(&hmac->hash.sha3, (byte*)hmac->ipad,
                                                        WC_SHA3_384_BLOCK_SIZE);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            ret = wc_Sha3_512_Update(&hmac->hash.sha3, (byte*)hmac->ipad,
                                                        WC_SHA3_512_BLOCK_SIZE);
            break;
    #endif
    #endif /* WOLFSSL_SHA3 */

        default:
            break;
    }

    if (ret == 0)
        hmac->innerHashKeyed = WC_HMAC_INNER_HASH_KEYED_SW;

    return ret;
}


int wc_HmacUpdate(Hmac* hmac, const byte* msg, word32 length)
{
    int ret = 0;

    if (hmac == NULL || (msg == NULL && length > 0)) {
        return BAD_FUNC_ARG;
    }

#ifdef WOLF_CRYPTO_CB
    if (hmac->devId != INVALID_DEVID) {
        ret = wc_CryptoCb_Hmac(hmac, hmac->macType, msg, length, NULL);
        if (ret != CRYPTOCB_UNAVAILABLE)
            return ret;
        /* fall-through when unavailable */
        ret = 0; /* reset error code */
    }
#endif
#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_HMAC)
    if (hmac->asyncDev.marker == WOLFSSL_ASYNC_MARKER_HMAC) {
    #if defined(HAVE_CAVIUM)
        return NitroxHmacUpdate(hmac, msg, length);
    #elif defined(HAVE_INTEL_QA)
        if (IntelQaHmacGetType(hmac->macType, NULL) == 0) {
            return IntelQaHmac(&hmac->asyncDev, hmac->macType,
                (byte*)hmac->ipad, hmac->keyLen, NULL, msg, length);
        }
    #endif
    }
#endif /* WOLFSSL_ASYNC_CRYPT */

    if (!hmac->innerHashKeyed) {
        ret = HmacKeyInnerHash(hmac);
        if (ret != 0)
            return ret;
    }

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            ret = wc_Md5Update(&hmac->hash.md5, msg, length);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_ShaUpdate(&hmac->hash.sha, msg, length);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            ret = wc_Sha224Update(&hmac->hash.sha224, msg, length);
            break;
    #endif /* WOLFSSL_SHA224 */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_Sha256Update(&hmac->hash.sha256, msg, length);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_Sha384Update(&hmac->hash.sha384, msg, length);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_Sha512Update(&hmac->hash.sha512, msg, length);
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            ret = wc_Sha3_224_Update(&hmac->hash.sha3, msg, length);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            ret = wc_Sha3_256_Update(&hmac->hash.sha3, msg, length);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            ret = wc_Sha3_384_Update(&hmac->hash.sha3, msg, length);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            ret = wc_Sha3_512_Update(&hmac->hash.sha3, msg, length);
            break;
    #endif
    #endif /* WOLFSSL_SHA3 */

        default:
            break;
    }

    return ret;
}


int wc_HmacFinal(Hmac* hmac, byte* hash)
{
    int ret;

    if (hmac == NULL || hash == NULL) {
        return BAD_FUNC_ARG;
    }

#ifdef WOLF_CRYPTO_CB
    if (hmac->devId != INVALID_DEVID) {
        ret = wc_CryptoCb_Hmac(hmac, hmac->macType, NULL, 0, hash);
        if (ret != CRYPTOCB_UNAVAILABLE)
            return ret;
        /* fall-through when unavailable */
    }
#endif
#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_HMAC)
    if (hmac->asyncDev.marker == WOLFSSL_ASYNC_MARKER_HMAC) {
        int hashLen = wc_HmacSizeByType(hmac->macType);
        if (hashLen <= 0)
            return hashLen;

    #if defined(HAVE_CAVIUM)
        return NitroxHmacFinal(hmac, hash, hashLen);
    #elif defined(HAVE_INTEL_QA)
        if (IntelQaHmacGetType(hmac->macType, NULL) == 0) {
            return IntelQaHmac(&hmac->asyncDev, hmac->macType,
                (byte*)hmac->ipad, hmac->keyLen, hash, NULL, hashLen);
        }
    #endif
    }
#endif /* WOLFSSL_ASYNC_CRYPT */

    if (!hmac->innerHashKeyed) {
        ret = HmacKeyInnerHash(hmac);
        if (ret != 0)
            return ret;
    }

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            ret = wc_Md5Final(&hmac->hash.md5, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Md5Update(&hmac->hash.md5, (byte*)hmac->opad,
                                                             WC_MD5_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Md5Update(&hmac->hash.md5, (byte*)hmac->innerHash,
                                                            WC_MD5_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Md5Final(&hmac->hash.md5, hash);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_ShaFinal(&hmac->hash.sha, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_ShaUpdate(&hmac->hash.sha, (byte*)hmac->opad,
                                                             WC_SHA_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_ShaUpdate(&hmac->hash.sha, (byte*)hmac->innerHash,
                                                            WC_SHA_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_ShaFinal(&hmac->hash.sha, hash);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            ret = wc_Sha224Final(&hmac->hash.sha224, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha224Update(&hmac->hash.sha224, (byte*)hmac->opad,
                                                          WC_SHA224_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha224Update(&hmac->hash.sha224, (byte*)hmac->innerHash,
                                                         WC_SHA224_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha224Final(&hmac->hash.sha224, hash);
            if (ret != 0)
                break;
            break;
    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_Sha256Final(&hmac->hash.sha256, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha256Update(&hmac->hash.sha256, (byte*)hmac->opad,
                                                          WC_SHA256_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha256Update(&hmac->hash.sha256, (byte*)hmac->innerHash,
                                                         WC_SHA256_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha256Final(&hmac->hash.sha256, hash);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_Sha384Final(&hmac->hash.sha384, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha384Update(&hmac->hash.sha384, (byte*)hmac->opad,
                                                          WC_SHA384_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha384Update(&hmac->hash.sha384, (byte*)hmac->innerHash,
                                                         WC_SHA384_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha384Final(&hmac->hash.sha384, hash);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_Sha512Final(&hmac->hash.sha512, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha512Update(&hmac->hash.sha512, (byte*)hmac->opad,
                                                          WC_SHA512_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha512Update(&hmac->hash.sha512, (byte*)hmac->innerHash,
                                                         WC_SHA512_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha512Final(&hmac->hash.sha512, hash);
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            ret = wc_Sha3_224_Final(&hmac->hash.sha3, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha3_224_Update(&hmac->hash.sha3, (byte*)hmac->opad,
                                                        WC_SHA3_224_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_224_Update(&hmac->hash.sha3, (byte*)hmac->innerHash,
                                                       WC_SHA3_224_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_224_Final(&hmac->hash.sha3, hash);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            ret = wc_Sha3_256_Final(&hmac->hash.sha3, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha3_256_Update(&hmac->hash.sha3, (byte*)hmac->opad,
                                                        WC_SHA3_256_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_256_Update(&hmac->hash.sha3, (byte*)hmac->innerHash,
                                                       WC_SHA3_256_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_256_Final(&hmac->hash.sha3, hash);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            ret = wc_Sha3_384_Final(&hmac->hash.sha3, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha3_384_Update(&hmac->hash.sha3, (byte*)hmac->opad,
                                                        WC_SHA3_384_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_384_Update(&hmac->hash.sha3, (byte*)hmac->innerHash,
                                                       WC_SHA3_384_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_384_Final(&hmac->hash.sha3, hash);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            ret = wc_Sha3_512_Final(&hmac->hash.sha3, (byte*)hmac->innerHash);
            if (ret != 0)
                break;
            ret = wc_Sha3_512_Update(&hmac->hash.sha3, (byte*)hmac->opad,
                                                        WC_SHA3_512_BLOCK_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_512_Update(&hmac->hash.sha3, (byte*)hmac->innerHash,
                                                       WC_SHA3_512_DIGEST_SIZE);
            if (ret != 0)
                break;
            ret = wc_Sha3_512_Final(&hmac->hash.sha3, hash);
            break;
    #endif
    #endif /* WOLFSSL_SHA3 */

        default:
            ret = BAD_FUNC_ARG;
            break;
    }

    if (ret == 0) {
        hmac->innerHashKeyed = 0;
    }

    return ret;
}

#ifdef WOLFSSL_KCAPI_HMAC
    /* implemented in wolfcrypt/src/port/kcapi/kcapi_hmac.c */

#else
/* Initialize Hmac for use with async device */
int wc_HmacInit(Hmac* hmac, void* heap, int devId)
{
    int ret = 0;

    if (hmac == NULL)
        return BAD_FUNC_ARG;

    XMEMSET(hmac, 0, sizeof(Hmac));
    hmac->macType = WC_HASH_TYPE_NONE;
    hmac->heap = heap;
#ifdef WOLF_CRYPTO_CB
    hmac->devId = devId;
    hmac->devCtx = NULL;
#endif

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_HMAC)
    ret = wolfAsync_DevCtxInit(&hmac->asyncDev, WOLFSSL_ASYNC_MARKER_HMAC,
                                                         hmac->heap, devId);
#else
    (void)devId;
#endif /* WOLFSSL_ASYNC_CRYPT */

    return ret;
}

#ifdef HAVE_PKCS11
int  wc_HmacInit_Id(Hmac* hmac, unsigned char* id, int len, void* heap,
                    int devId)
{
    int ret = 0;

    if (hmac == NULL)
        ret = BAD_FUNC_ARG;
    if (ret == 0 && (len < 0 || len > HMAC_MAX_ID_LEN))
        ret = BUFFER_E;

    if (ret == 0)
        ret = wc_HmacInit(hmac, heap, devId);
    if (ret == 0) {
        XMEMCPY(hmac->id, id, len);
        hmac->idLen = len;
    }

    return ret;
}

int wc_HmacInit_Label(Hmac* hmac, const char* label, void* heap, int devId)
{
    int ret = 0;
    int labelLen = 0;

    if (hmac == NULL || label == NULL)
        ret = BAD_FUNC_ARG;
    if (ret == 0) {
        labelLen = (int)XSTRLEN(label);
        if (labelLen == 0 || labelLen > HMAC_MAX_LABEL_LEN)
            ret = BUFFER_E;
    }

    if (ret == 0)
        ret  = wc_HmacInit(hmac, heap, devId);
    if (ret == 0) {
        XMEMCPY(hmac->label, label, labelLen);
        hmac->labelLen = labelLen;
    }

    return ret;
}
#endif

/* Free Hmac from use with async device */
void wc_HmacFree(Hmac* hmac)
{
    if (hmac == NULL)
        return;

#ifdef WOLF_CRYPTO_CB
    /* handle cleanup case where final is not called */
    if (hmac->devId != INVALID_DEVID && hmac->devCtx != NULL) {
        int  ret;
        byte finalHash[WC_HMAC_BLOCK_SIZE];
        ret = wc_CryptoCb_Hmac(hmac, hmac->macType, NULL, 0, finalHash);
        (void)ret; /* must ignore return code here */
        (void)finalHash;
    }
#endif

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            wc_Md5Free(&hmac->hash.md5);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            wc_ShaFree(&hmac->hash.sha);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            wc_Sha224Free(&hmac->hash.sha224);
            break;
    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
        case WC_SHA256:
            wc_Sha256Free(&hmac->hash.sha256);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            wc_Sha384Free(&hmac->hash.sha384);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            wc_Sha512Free(&hmac->hash.sha512);
            break;
    #endif /* WOLFSSL_SHA512 */

    #ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
        case WC_SHA3_224:
            wc_Sha3_224_Free(&hmac->hash.sha3);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_256
        case WC_SHA3_256:
            wc_Sha3_256_Free(&hmac->hash.sha3);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_384
        case WC_SHA3_384:
            wc_Sha3_384_Free(&hmac->hash.sha3);
            break;
    #endif
    #ifndef WOLFSSL_NOSHA3_512
        case WC_SHA3_512:
            wc_Sha3_512_Free(&hmac->hash.sha3);
            break;
    #endif
    #endif /* WOLFSSL_SHA3 */

        default:
            break;
    }

#if defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_HMAC)
    wolfAsync_DevCtxFree(&hmac->asyncDev, WOLFSSL_ASYNC_MARKER_HMAC);
#endif /* WOLFSSL_ASYNC_CRYPT */

    switch (hmac->macType) {
    #ifndef NO_MD5
        case WC_MD5:
            wc_Md5Free(&hmac->hash.md5);
            break;
    #endif /* !NO_MD5 */

    #ifndef NO_SHA
        case WC_SHA:
            wc_ShaFree(&hmac->hash.sha);
            break;
    #endif /* !NO_SHA */

    #ifdef WOLFSSL_SHA224
        case WC_SHA224:
            wc_Sha224Free(&hmac->hash.sha224);
            break;
    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
        case WC_SHA256:
            wc_Sha256Free(&hmac->hash.sha256);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA512
    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            wc_Sha384Free(&hmac->hash.sha384);
            break;
    #endif /* WOLFSSL_SHA384 */
        case WC_SHA512:
            wc_Sha512Free(&hmac->hash.sha512);
            break;
    #endif /* WOLFSSL_SHA512 */
        default:
            break;
    }
}
#endif /* WOLFSSL_KCAPI_HMAC */

int wolfSSL_GetHmacMaxSize(void)
{
    return WC_MAX_DIGEST_SIZE;
}


#if defined(WOLFSSL_HAVE_PRF) && defined(HAVE_FIPS) && \
    defined(HAVE_FIPS_VERSION) && (HAVE_FIPS_VERSION >= 4)

#ifdef WOLFSSL_SHA512
    #define P_HASH_MAX_SIZE WC_SHA512_DIGEST_SIZE
#elif defined(WOLFSSL_SHA384)
    #define P_HASH_MAX_SIZE WC_SHA384_DIGEST_SIZE
#else
    #define P_HASH_MAX_SIZE WC_SHA256_DIGEST_SIZE
#endif

/* Pseudo Random Function for MD5, SHA-1, SHA-256, SHA-384, or SHA-512 */
int wc_PRF(byte* result, word32 resLen, const byte* secret,
                  word32 secLen, const byte* seed, word32 seedLen, int hash,
                  void* heap, int devId)
{
    word32 len = P_HASH_MAX_SIZE;
    word32 times;
    word32 lastLen;
    word32 lastTime;
    word32 i;
    word32 idx = 0;
    int    ret = 0;
#ifdef WOLFSSL_SMALL_STACK
    byte*  previous;
    byte*  current;
    Hmac*  hmac;
#else
    byte   previous[P_HASH_MAX_SIZE];  /* max size */
    byte   current[P_HASH_MAX_SIZE];   /* max size */
    Hmac   hmac[1];
#endif

#ifdef WOLFSSL_SMALL_STACK
    previous = (byte*)XMALLOC(P_HASH_MAX_SIZE, heap, DYNAMIC_TYPE_DIGEST);
    current  = (byte*)XMALLOC(P_HASH_MAX_SIZE, heap, DYNAMIC_TYPE_DIGEST);
    hmac     = (Hmac*)XMALLOC(sizeof(Hmac),    heap, DYNAMIC_TYPE_HMAC);

    if (previous == NULL || current == NULL || hmac == NULL) {
        if (previous) XFREE(previous, heap, DYNAMIC_TYPE_DIGEST);
        if (current)  XFREE(current,  heap, DYNAMIC_TYPE_DIGEST);
        if (hmac)     XFREE(hmac,     heap, DYNAMIC_TYPE_HMAC);

        return MEMORY_E;
    }
#endif

    switch (hash) {
    #ifndef NO_MD5
        case md5_mac:
            hash = WC_MD5;
            len  = WC_MD5_DIGEST_SIZE;
        break;
    #endif

    #ifndef NO_SHA256
        case sha256_mac:
            hash = WC_SHA256;
            len  = WC_SHA256_DIGEST_SIZE;
        break;
    #endif

    #ifdef WOLFSSL_SHA384
        case sha384_mac:
            hash = WC_SHA384;
            len  = WC_SHA384_DIGEST_SIZE;
        break;
    #endif

    #ifdef WOLFSSL_SHA512
        case sha512_mac:
            hash = WC_SHA512;
            len  = WC_SHA512_DIGEST_SIZE;
        break;
    #endif

    #ifndef NO_SHA
        case sha_mac:
        default:
            hash = WC_SHA;
            len  = WC_SHA_DIGEST_SIZE;
        break;
    #endif
    }

    times   = resLen / len;
    lastLen = resLen % len;

    if (lastLen)
        times += 1;

    lastTime = times - 1;

    ret = wc_HmacInit(hmac, heap, devId);
    if (ret == 0) {
        ret = wc_HmacSetKey(hmac, hash, secret, secLen);
        if (ret == 0)
            ret = wc_HmacUpdate(hmac, seed, seedLen); /* A0 = seed */
        if (ret == 0)
            ret = wc_HmacFinal(hmac, previous);       /* A1 */
        if (ret == 0) {
            for (i = 0; i < times; i++) {
                ret = wc_HmacUpdate(hmac, previous, len);
                if (ret != 0)
                    break;
                ret = wc_HmacUpdate(hmac, seed, seedLen);
                if (ret != 0)
                    break;
                ret = wc_HmacFinal(hmac, current);
                if (ret != 0)
                    break;

                if ((i == lastTime) && lastLen)
                    XMEMCPY(&result[idx], current,
                                             min(lastLen, P_HASH_MAX_SIZE));
                else {
                    XMEMCPY(&result[idx], current, len);
                    idx += len;
                    ret = wc_HmacUpdate(hmac, previous, len);
                    if (ret != 0)
                        break;
                    ret = wc_HmacFinal(hmac, previous);
                    if (ret != 0)
                        break;
                }
            }
        }
        wc_HmacFree(hmac);
    }

    ForceZero(previous,  P_HASH_MAX_SIZE);
    ForceZero(current,   P_HASH_MAX_SIZE);
    ForceZero(hmac,      sizeof(Hmac));

#ifdef WOLFSSL_SMALL_STACK
    XFREE(previous, heap, DYNAMIC_TYPE_DIGEST);
    XFREE(current,  heap, DYNAMIC_TYPE_DIGEST);
    XFREE(hmac,     heap, DYNAMIC_TYPE_HMAC);
#endif

    return ret;
}
#undef P_HASH_MAX_SIZE

/* compute PRF (pseudo random function) using SHA1 and MD5 for TLSv1 */
int wc_PRF_TLSv1(byte* digest, word32 digLen, const byte* secret,
           word32 secLen, const byte* label, word32 labLen,
           const byte* seed, word32 seedLen, void* heap, int devId)
{
    int    ret  = 0;
    word32 half = (secLen + 1) / 2;

#ifdef WOLFSSL_SMALL_STACK
    byte* md5_half;
    byte* sha_half;
    byte* md5_result;
    byte* sha_result;
#else
    byte  md5_half[MAX_PRF_HALF];     /* half is real size */
    byte  sha_half[MAX_PRF_HALF];     /* half is real size */
    byte  md5_result[MAX_PRF_DIG];    /* digLen is real size */
    byte  sha_result[MAX_PRF_DIG];    /* digLen is real size */
#endif
#if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
    DECLARE_VAR(labelSeed, byte, MAX_PRF_LABSEED, heap);
    if (labelSeed == NULL)
        return MEMORY_E;
#else
    byte labelSeed[MAX_PRF_LABSEED];
#endif

    if (half > MAX_PRF_HALF ||
        labLen + seedLen > MAX_PRF_LABSEED ||
        digLen > MAX_PRF_DIG)
    {
    #if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
        FREE_VAR(labelSeed, heap);
    #endif
        return BUFFER_E;
    }

#ifdef WOLFSSL_SMALL_STACK
    md5_half   = (byte*)XMALLOC(MAX_PRF_HALF,    heap, DYNAMIC_TYPE_DIGEST);
    sha_half   = (byte*)XMALLOC(MAX_PRF_HALF,    heap, DYNAMIC_TYPE_DIGEST);
    md5_result = (byte*)XMALLOC(MAX_PRF_DIG,     heap, DYNAMIC_TYPE_DIGEST);
    sha_result = (byte*)XMALLOC(MAX_PRF_DIG,     heap, DYNAMIC_TYPE_DIGEST);

    if (md5_half == NULL || sha_half == NULL || md5_result == NULL ||
                                                           sha_result == NULL) {
        if (md5_half)   XFREE(md5_half,   heap, DYNAMIC_TYPE_DIGEST);
        if (sha_half)   XFREE(sha_half,   heap, DYNAMIC_TYPE_DIGEST);
        if (md5_result) XFREE(md5_result, heap, DYNAMIC_TYPE_DIGEST);
        if (sha_result) XFREE(sha_result, heap, DYNAMIC_TYPE_DIGEST);
    #if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
        FREE_VAR(labelSeed, heap);
    #endif

        return MEMORY_E;
    }
#endif

    XMEMSET(md5_result, 0, digLen);
    XMEMSET(sha_result, 0, digLen);

    XMEMCPY(md5_half, secret, half);
    XMEMCPY(sha_half, secret + half - secLen % 2, half);

    XMEMCPY(labelSeed, label, labLen);
    XMEMCPY(labelSeed + labLen, seed, seedLen);

    if ((ret = wc_PRF(md5_result, digLen, md5_half, half, labelSeed,
                                labLen + seedLen, md5_mac, heap, devId)) == 0) {
        if ((ret = wc_PRF(sha_result, digLen, sha_half, half, labelSeed,
                                labLen + seedLen, sha_mac, heap, devId)) == 0) {
            /* calculate XOR for TLSv1 PRF */
            XMEMCPY(digest, md5_result, digLen);
            xorbuf(digest, sha_result, digLen);
        }
    }

#ifdef WOLFSSL_SMALL_STACK
    XFREE(md5_half,   heap, DYNAMIC_TYPE_DIGEST);
    XFREE(sha_half,   heap, DYNAMIC_TYPE_DIGEST);
    XFREE(md5_result, heap, DYNAMIC_TYPE_DIGEST);
    XFREE(sha_result, heap, DYNAMIC_TYPE_DIGEST);
#endif

#if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
    FREE_VAR(labelSeed, heap);
#endif

    return ret;
}

/* Wrapper for TLS 1.2 and TLSv1 cases to calculate PRF */
/* In TLS 1.2 case call straight thru to wc_PRF */
int wc_PRF_TLS(byte* digest, word32 digLen, const byte* secret, word32 secLen,
            const byte* label, word32 labLen, const byte* seed, word32 seedLen,
            int useAtLeastSha256, int hash_type, void* heap, int devId)
{
    int ret = 0;

    if (useAtLeastSha256) {
    #if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
        DECLARE_VAR(labelSeed, byte, MAX_PRF_LABSEED, heap);
        if (labelSeed == NULL)
            return MEMORY_E;
    #else
        byte labelSeed[MAX_PRF_LABSEED];
    #endif

        if (labLen + seedLen > MAX_PRF_LABSEED)
            return BUFFER_E;

        XMEMCPY(labelSeed, label, labLen);
        XMEMCPY(labelSeed + labLen, seed, seedLen);

        /* If a cipher suite wants an algorithm better than sha256, it
         * should use better. */
        if (hash_type < sha256_mac || hash_type == blake2b_mac)
            hash_type = sha256_mac;
        /* compute PRF for MD5, SHA-1, SHA-256, or SHA-384 for TLSv1.2 PRF */
        ret = wc_PRF(digest, digLen, secret, secLen, labelSeed,
                     labLen + seedLen, hash_type, heap, devId);

    #if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_ASYNC_NO_HASH)
        FREE_VAR(labelSeed, heap);
    #endif
    }
#ifndef NO_OLD_TLS
    else {
        /* compute TLSv1 PRF (pseudo random function using HMAC) */
        ret = wc_PRF_TLSv1(digest, digLen, secret, secLen, label, labLen, seed,
                          seedLen, heap, devId);
    }
#endif

    return ret;
}
#endif /* WOLFSSL_HAVE_PRF */


#ifdef HAVE_HKDF
    /* HMAC-KDF-Extract.
     * RFC 5869 - HMAC-based Extract-and-Expand Key Derivation Function (HKDF).
     *
     * type     The hash algorithm type.
     * salt     The optional salt value.
     * saltSz   The size of the salt.
     * inKey    The input keying material.
     * inKeySz  The size of the input keying material.
     * out      The pseudorandom key with the length that of the hash.
     * returns 0 on success, otherwise failure.
     */
    int wc_HKDF_Extract(int type, const byte* salt, word32 saltSz,
                        const byte* inKey, word32 inKeySz, byte* out)
    {
        byte   tmp[WC_MAX_DIGEST_SIZE]; /* localSalt helper */
        Hmac   myHmac;
        int    ret;
        const  byte* localSalt;  /* either points to user input or tmp */
        int    hashSz;

        ret = wc_HmacSizeByType(type);
        if (ret < 0)
            return ret;

        hashSz = ret;
        localSalt = salt;
        if (localSalt == NULL) {
            XMEMSET(tmp, 0, hashSz);
            localSalt = tmp;
            saltSz    = hashSz;
        }

        ret = wc_HmacInit(&myHmac, NULL, INVALID_DEVID);
        if (ret == 0) {
            ret = wc_HmacSetKey(&myHmac, type, localSalt, saltSz);
            if (ret == 0)
                ret = wc_HmacUpdate(&myHmac, inKey, inKeySz);
            if (ret == 0)
                ret = wc_HmacFinal(&myHmac,  out);
            wc_HmacFree(&myHmac);
        }

        return ret;
    }

    /* HMAC-KDF-Expand.
     * RFC 5869 - HMAC-based Extract-and-Expand Key Derivation Function (HKDF).
     *
     * type     The hash algorithm type.
     * inKey    The input key.
     * inKeySz  The size of the input key.
     * info     The application specific information.
     * infoSz   The size of the application specific information.
     * out      The output keying material.
     * returns 0 on success, otherwise failure.
     */
    int wc_HKDF_Expand(int type, const byte* inKey, word32 inKeySz,
                       const byte* info, word32 infoSz, byte* out, word32 outSz)
    {
        byte   tmp[WC_MAX_DIGEST_SIZE];
        Hmac   myHmac;
        int    ret = 0;
        word32 outIdx = 0;
        word32 hashSz = wc_HmacSizeByType(type);
        byte   n = 0x1;

        /* RFC 5869 states that the length of output keying material in
           octets must be L <= 255*HashLen or N = ceil(L/HashLen) */

        if (out == NULL || ((outSz/hashSz) + ((outSz % hashSz) != 0)) > 255)
            return BAD_FUNC_ARG;

        ret = wc_HmacInit(&myHmac, NULL, INVALID_DEVID);
        if (ret != 0)
            return ret;


        while (outIdx < outSz) {
            int    tmpSz = (n == 1) ? 0 : hashSz;
            word32 left = outSz - outIdx;

            ret = wc_HmacSetKey(&myHmac, type, inKey, inKeySz);
            if (ret != 0)
                break;
            ret = wc_HmacUpdate(&myHmac, tmp, tmpSz);
            if (ret != 0)
                break;
            ret = wc_HmacUpdate(&myHmac, info, infoSz);
            if (ret != 0)
                break;
            ret = wc_HmacUpdate(&myHmac, &n, 1);
            if (ret != 0)
                break;
            ret = wc_HmacFinal(&myHmac, tmp);
            if (ret != 0)
                break;

            left = min(left, hashSz);
            XMEMCPY(out+outIdx, tmp, left);

            outIdx += hashSz;
            n++;
        }

        wc_HmacFree(&myHmac);

        return ret;
    }

    /* HMAC-KDF.
     * RFC 5869 - HMAC-based Extract-and-Expand Key Derivation Function (HKDF).
     *
     * type     The hash algorithm type.
     * inKey    The input keying material.
     * inKeySz  The size of the input keying material.
     * salt     The optional salt value.
     * saltSz   The size of the salt.
     * info     The application specific information.
     * infoSz   The size of the application specific information.
     * out      The output keying material.
     * returns 0 on success, otherwise failure.
     */
    int wc_HKDF(int type, const byte* inKey, word32 inKeySz,
                       const byte* salt,  word32 saltSz,
                       const byte* info,  word32 infoSz,
                       byte* out,         word32 outSz)
    {
        byte   prk[WC_MAX_DIGEST_SIZE];
        int    hashSz = wc_HmacSizeByType(type);
        int    ret;

        if (hashSz < 0)
            return BAD_FUNC_ARG;

        ret = wc_HKDF_Extract(type, salt, saltSz, inKey, inKeySz, prk);
        if (ret != 0)
            return ret;

        return wc_HKDF_Expand(type, prk, hashSz, info, infoSz, out, outSz);
    }

#if !defined(HAVE_FIPS) || \
    defined(HAVE_FIPS_VERSION) && (HAVE_FIPS_VERSION >= 4)
    /* Extract data using HMAC, salt and input.
     * RFC 5869 - HMAC-based Extract-and-Expand Key Derivation Function (HKDF)
     *
     * prk      The generated pseudorandom key.
     * salt     The salt.
     * saltLen  The length of the salt.
     * ikm      The input keying material.
     * ikmLen   The length of the input keying material.
     * digest   The type of digest to use.
     * returns 0 on success, otherwise failure.
     */
    int wc_Tls13_HKDF_Extract(byte* prk, const byte* salt, int saltLen,
                                 byte* ikm, int ikmLen, int digest)
    {
        int ret;
        int len = 0;

        switch (digest) {
            #ifndef NO_SHA256
            case WC_SHA256:
                len = WC_SHA256_DIGEST_SIZE;
                break;
            #endif

            #ifdef WOLFSSL_SHA384
            case WC_SHA384:
                len = WC_SHA384_DIGEST_SIZE;
                break;
            #endif

            #ifdef WOLFSSL_TLS13_SHA512
            case WC_SHA512:
                len = WC_SHA512_DIGEST_SIZE;
                break;
            #endif
            default:
                return BAD_FUNC_ARG;
        }

        /* When length is 0 then use zeroed data of digest length. */
        if (ikmLen == 0) {
            ikmLen = len;
            XMEMSET(ikm, 0, len);
        }

#ifdef WOLFSSL_DEBUG_TLS
        WOLFSSL_MSG("  Salt");
        WOLFSSL_BUFFER(salt, saltLen);
        WOLFSSL_MSG("  IKM");
        WOLFSSL_BUFFER(ikm, ikmLen);
#endif

        ret = wc_HKDF_Extract(digest, salt, saltLen, ikm, ikmLen, prk);

#ifdef WOLFSSL_DEBUG_TLS
        WOLFSSL_MSG("  PRK");
        WOLFSSL_BUFFER(prk, len);
#endif

        return ret;
    }

    /* Expand data using HMAC, salt and label and info.
     * TLS v1.3 defines this function.
     *
     * okm          The generated pseudorandom key - output key material.
     * okmLen       The length of generated pseudorandom key -
     *              output key material.
     * prk          The salt - pseudo-random key.
     * prkLen       The length of the salt - pseudo-random key.
     * protocol     The TLS protocol label.
     * protocolLen  The length of the TLS protocol label.
     * info         The information to expand.
     * infoLen      The length of the information.
     * digest       The type of digest to use.
     * returns 0 on success, otherwise failure.
     */
    int wc_Tls13_HKDF_Expand_Label(byte* okm, word32 okmLen,
                                 const byte* prk, word32 prkLen,
                                 const byte* protocol, word32 protocolLen,
                                 const byte* label, word32 labelLen,
                                 const byte* info, word32 infoLen,
                                 int digest)
    {
        int    ret = 0;
        int    idx = 0;
        byte   data[MAX_TLS13_HKDF_LABEL_SZ];

        /* Output length. */
        data[idx++] = (byte)(okmLen >> 8);
        data[idx++] = (byte)okmLen;
        /* Length of protocol | label. */
        data[idx++] = (byte)(protocolLen + labelLen);
        /* Protocol */
        XMEMCPY(&data[idx], protocol, protocolLen);
        idx += protocolLen;
        /* Label */
        XMEMCPY(&data[idx], label, labelLen);
        idx += labelLen;
        /* Length of hash of messages */
        data[idx++] = (byte)infoLen;
        /* Hash of messages */
        XMEMCPY(&data[idx], info, infoLen);
        idx += infoLen;

#ifdef WOLFSSL_DEBUG_TLS
        WOLFSSL_MSG("  PRK");
        WOLFSSL_BUFFER(prk, prkLen);
        WOLFSSL_MSG("  Info");
        WOLFSSL_BUFFER(data, idx);
#endif

        ret = wc_HKDF_Expand(digest, prk, prkLen, data, idx, okm, okmLen);

#ifdef WOLFSSL_DEBUG_TLS
        WOLFSSL_MSG("  OKM");
        WOLFSSL_BUFFER(okm, okmLen);
#endif

        ForceZero(data, idx);

        return ret;
    }
#endif /* newer HAVE_FIPS */

#endif /* HAVE_HKDF */


#ifdef WOLFSSL_WOLFSSH

static
int _HashInit(byte hashId, wc_Hmac_Hash* hash)
{
    int ret = BAD_FUNC_ARG;

    switch (hashId) {
    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_InitSha(&hash->sha);
            break;
    #endif /* !NO_SHA */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_InitSha256(&hash->sha256);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_InitSha384(&hash->sha384);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_InitSha512(&hash->sha512);
            break;
    #endif /* WOLFSSL_SHA512 */
    }

    return ret;
}

static
int _HashUpdate(byte hashId, wc_Hmac_Hash* hash,
        const byte* data, word32 dataSz)
{
    int ret = BAD_FUNC_ARG;

    switch (hashId) {
    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_ShaUpdate(&hash->sha, data, dataSz);
            break;
    #endif /* !NO_SHA */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_Sha256Update(&hash->sha256, data, dataSz);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_Sha384Update(&hash->sha384, data, dataSz);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_Sha512Update(&hash->sha512, data, dataSz);
            break;
    #endif /* WOLFSSL_SHA512 */
    }

    return ret;
}

static
int _HashFinal(byte hashId, wc_Hmac_Hash* hash, byte* digest)
{
    int ret = BAD_FUNC_ARG;

    switch (hashId) {
    #ifndef NO_SHA
        case WC_SHA:
            ret = wc_ShaFinal(&hash->sha, digest);
            break;
    #endif /* !NO_SHA */

    #ifndef NO_SHA256
        case WC_SHA256:
            ret = wc_Sha256Final(&hash->sha256, digest);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            ret = wc_Sha384Final(&hash->sha384, digest);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            ret = wc_Sha512Final(&hash->sha512, digest);
            break;
    #endif /* WOLFSSL_SHA512 */
    }

    return ret;
}

static
void _HashFree(byte hashId, wc_Hmac_Hash* hash)
{
    switch (hashId) {
    #ifndef NO_SHA
        case WC_SHA:
            wc_ShaFree(&hash->sha);
            break;
    #endif /* !NO_SHA */

    #ifndef NO_SHA256
        case WC_SHA256:
            wc_Sha256Free(&hash->sha256);
            break;
    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA384
        case WC_SHA384:
            wc_Sha384Free(&hash->sha384);
            break;
    #endif /* WOLFSSL_SHA384 */
    #ifdef WOLFSSL_SHA512
        case WC_SHA512:
            wc_Sha512Free(&hash->sha512);
            break;
    #endif /* WOLFSSL_SHA512 */
    }
}


#define LENGTH_SZ 4

int wc_SSH_KDF(byte hashId, byte keyId, byte* key, word32 keySz,
        const byte* k, word32 kSz, const byte* h, word32 hSz,
        const byte* sessionId, word32 sessionIdSz)
{
    word32 blocks, remainder;
    wc_Hmac_Hash hash;
    enum wc_HashType enmhashId = (enum wc_HashType)hashId;
    byte kPad = 0;
    byte pad = 0;
    byte kSzFlat[LENGTH_SZ];
    int digestSz;
    int ret;

    if (key == NULL || keySz == 0 ||
        k == NULL || kSz == 0 ||
        h == NULL || hSz == 0 ||
        sessionId == NULL || sessionIdSz == 0) {

        return BAD_FUNC_ARG;
    }

    digestSz = wc_HmacSizeByType(enmhashId);
    if (digestSz < 0) {
        return BAD_FUNC_ARG;
    }

    if (k[0] & 0x80) kPad = 1;
    c32toa(kSz + kPad, kSzFlat);

    blocks = keySz / digestSz;
    remainder = keySz % digestSz;

    ret = _HashInit(enmhashId, &hash);
    if (ret == 0)
        ret = _HashUpdate(enmhashId, &hash, kSzFlat, LENGTH_SZ);
    if (ret == 0 && kPad)
        ret = _HashUpdate(enmhashId, &hash, &pad, 1);
    if (ret == 0)
        ret = _HashUpdate(enmhashId, &hash, k, kSz);
    if (ret == 0)
        ret = _HashUpdate(enmhashId, &hash, h, hSz);
    if (ret == 0)
        ret = _HashUpdate(enmhashId, &hash, &keyId, sizeof(keyId));
    if (ret == 0)
        ret = _HashUpdate(enmhashId, &hash, sessionId, sessionIdSz);

    if (ret == 0) {
        if (blocks == 0) {
            if (remainder > 0) {
                byte lastBlock[WC_MAX_DIGEST_SIZE];
                ret = _HashFinal(enmhashId, &hash, lastBlock);
                if (ret == 0)
                    XMEMCPY(key, lastBlock, remainder);
            }
        }
        else {
            word32 runningKeySz, curBlock;

            runningKeySz = digestSz;
            ret = _HashFinal(enmhashId, &hash, key);

            for (curBlock = 1; curBlock < blocks; curBlock++) {
                ret = _HashInit(enmhashId, &hash);
                if (ret != 0) break;
                ret = _HashUpdate(enmhashId, &hash, kSzFlat, LENGTH_SZ);
                if (ret != 0) break;
                if (kPad)
                    ret = _HashUpdate(enmhashId, &hash, &pad, 1);
                if (ret != 0) break;
                ret = _HashUpdate(enmhashId, &hash, k, kSz);
                if (ret != 0) break;
                ret = _HashUpdate(enmhashId, &hash, h, hSz);
                if (ret != 0) break;
                ret = _HashUpdate(enmhashId, &hash, key, runningKeySz);
                if (ret != 0) break;
                ret = _HashFinal(enmhashId, &hash, key + runningKeySz);
                if (ret != 0) break;
                runningKeySz += digestSz;
            }

            if (remainder > 0) {
                byte lastBlock[WC_MAX_DIGEST_SIZE];
                if (ret == 0)
                    ret = _HashInit(enmhashId, &hash);
                if (ret == 0)
                    ret = _HashUpdate(enmhashId, &hash, kSzFlat, LENGTH_SZ);
                if (ret == 0 && kPad)
                    ret = _HashUpdate(enmhashId, &hash, &pad, 1);
                if (ret == 0)
                    ret = _HashUpdate(enmhashId, &hash, k, kSz);
                if (ret == 0)
                    ret = _HashUpdate(enmhashId, &hash, h, hSz);
                if (ret == 0)
                    ret = _HashUpdate(enmhashId, &hash, key, runningKeySz);
                if (ret == 0)
                    ret = _HashFinal(enmhashId, &hash, lastBlock);
                if (ret == 0)
                    XMEMCPY(key + runningKeySz, lastBlock, remainder);
            }
        }
    }

    _HashFree(enmhashId, &hash);

    return ret;
}

#endif /* WOLFSSL_WOLFSSH */

#endif /* HAVE_FIPS */
#endif /* NO_HMAC */
