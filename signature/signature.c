/* signature.c
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.
 *
 * This file is part of wolfSSL. (formerly known as CyaSSL)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/signature.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/error-crypt.h>

#define RSA_KEY_SIZE    2048

void hexdump(const void *buffer, word32 len, byte cols)
{
   word32 i;

   for(i = 0; i < len + ((len % cols) ? (cols - len % cols) : 0); i++)
   {
      /* print hex data */
      if(i < len) {
         printf("%02X ", ((byte*)buffer)[i] & 0xFF);
      }

      if(i % cols == (cols - 1)) {
         printf("\n");
      }
   }
}

int ecc_sign_verify_test(enum wc_HashType hash_type, enum wc_SignatureType sig_type,
    byte* fileBuf, int fileLen)
{
    int ret;
    ecc_key eccKey;
    RNG rng;
    byte* sigBuf = NULL;
    word32 sigLen;
    byte eccPubKeyBuf[ECC_BUFSIZE], eccPrivKeyBuf[ECC_BUFSIZE];
    word32 eccPubKeyLen, eccPrivKeyLen;

    /* Init */
    wc_InitRng(&rng);

    /* Generate key */
    wc_ecc_init(&eccKey);
    ret = wc_ecc_make_key(&rng, 32, &eccKey);
    if(ret != 0) {
        printf("Make ECC Key Failed! %d\n", ret);
    }

    /* Display public key data */
    eccPubKeyLen = ECC_BUFSIZE;
    ret = wc_ecc_export_x963(&eccKey, eccPubKeyBuf, &eccPubKeyLen);
    if (ret != 0) {
        printf("ECC public key x963 export failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }
    printf("ECC Public Key: Len %d\n", eccPubKeyLen);
    hexdump(eccPubKeyBuf, eccPubKeyLen, 16);

    /* Display private key data */
    eccPrivKeyLen = ECC_BUFSIZE;
    ret = wc_ecc_export_private_only(&eccKey, eccPrivKeyBuf, &eccPrivKeyLen);
    if (ret != 0) {
        printf("ECC private key export failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }
    printf("ECC Private Key: Len %d\n", eccPrivKeyLen);
    hexdump(eccPrivKeyBuf, eccPrivKeyLen, 16);

    /* Get signature length and allocate buffer */
    sigLen = wc_SignatureGetSize(sig_type, &eccKey, sizeof(eccKey));
    if(sigLen <= 0) {
        printf("Signature type %d not supported!\n", sig_type);
        ret = EXIT_FAILURE;
        goto exit;
    }
    sigBuf = malloc(sigLen);
    if(!sigBuf) {
        printf("Signature malloc failed!\n");
        ret = EXIT_FAILURE;
        goto exit;
    }
    printf("Signature Len: %d\n", sigLen);

    /* Perform hash and sign to create signature */
    ret = wc_SignatureGenerate(
        hash_type, sig_type,
        fileBuf, fileLen,
        sigBuf, &sigLen,
        &eccKey, sizeof(eccKey),
        &rng);
    printf("Signature Generation: %s (%d)\n", (ret == 0) ? "Pass" : "Fail", ret);
    if(ret != 0) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    printf("Signature Data:\n");
    hexdump(sigBuf, sigLen, 16);

    /* Perform signature verification */
    /* Release and init new key */
    wc_ecc_free(&eccKey);
    wc_ecc_init(&eccKey);

    /* Import the public key */
    ret = wc_ecc_import_x963(eccPubKeyBuf, eccPubKeyLen, &eccKey);
    if (ret != 0) {
        printf("ECC public key import failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }

    /* Perform signature verification using public key */
    ret = wc_SignatureVerify(
        hash_type, sig_type,
        fileBuf, fileLen,
        sigBuf, sigLen,
        &eccKey, sizeof(eccKey));
    printf("Signature Verification: %s (%d)\n", (ret == 0) ? "Pass" : "Fail", ret);
    if(ret != 0) {
        ret = EXIT_FAILURE;
    }

exit:
    /* Free */
    if(sigBuf) {
        free(sigBuf);
    }
    wc_ecc_free(&eccKey);
    wc_FreeRng(&rng);

    return ret;
}

int rsa_load_der_file(const char* derFile, RsaKey *rsaKey)
{
    int ret = EXIT_FAILURE;
    FILE *file;
    byte *buffer = NULL;
    word32 bytes = 0;
    word32 idx = 0;

    file = fopen(derFile, "rb");
    if (file) {
        buffer = malloc(RSA_KEY_SIZE);
        if(buffer) {
            bytes = fread(buffer, 1, RSA_KEY_SIZE, file);
            fclose(file);        
        }
    }
    
    if(buffer != NULL && bytes > 0) {
        ret = wc_RsaPrivateKeyDecode(buffer, &idx, rsaKey, (word32)bytes);
    }
    
    if(buffer) {
        free(buffer);
    }    
    return ret;
}

int rsa_sign_verify_test(enum wc_HashType hash_type, enum wc_SignatureType sig_type,
    byte* fileBuf, int fileLen)
{
    int ret;
    RsaKey rsaKey;
    RNG rng;
    byte *sigBuf = NULL;
    word32 sigLen;
#ifdef WOLFSSL_KEY_GEN
    byte *rsaKeyBuf = NULL, *rsaPubKeyBuf = NULL;
    word32 rsaKeyLen, rsaPubKeyLen;
    word32 idx = 0;
#endif

    /* Init */
    wc_InitRng(&rng);

    /* Generate key */
    wc_InitRsaKey(&rsaKey, NULL);
#ifdef WOLFSSL_KEY_GEN
    ret = wc_MakeRsaKey(&rsaKey, RSA_KEY_SIZE, 65537, &rng);
    if(ret != 0) {
        printf("Make RSA Key Failed! %d\n", ret);
    }

    /* Display key data */
    rsaKeyLen = RSA_KEY_SIZE;
    rsaKeyBuf = malloc(rsaKeyLen);
    ret = wc_RsaKeyToDer(&rsaKey, rsaKeyBuf, rsaKeyLen);
    if (ret <= 0) {
        printf("RSA key Der export failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }
    rsaKeyLen = ret;
    printf("RSA Key: Len %d\n", rsaKeyLen);
    hexdump(rsaKeyBuf, rsaKeyLen, 16);
    
    rsaPubKeyLen = RSA_KEY_SIZE;
    rsaPubKeyBuf = malloc(rsaPubKeyLen);
    ret = wc_RsaKeyToPublicDer(&rsaKey, rsaPubKeyBuf, rsaPubKeyLen);
    if (ret <= 0) {
        printf("RSA public key Der export failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }
    rsaPubKeyLen = ret;
    printf("RSA Public Key: Len %d\n", rsaPubKeyLen);
    hexdump(rsaPubKeyBuf, rsaPubKeyLen, 16);
#else
    /* Load cert from file client-key.der */
    rsa_load_der_file("../certs/client-key.der", &rsaKey);
#endif

    /* Get signature length and allocate buffer */
    sigLen = wc_SignatureGetSize(sig_type, &rsaKey, sizeof(rsaKey));
    if(sigLen <= 0) {
        printf("Signature %d size check fail! %d\n", sig_type, sigLen);
        ret = EXIT_FAILURE;
        goto exit;
    }
    sigBuf = malloc(sigLen);
    if(!sigBuf) {
        printf("Signature malloc failed!\n");
        ret = EXIT_FAILURE;
        goto exit;
    }
    printf("Signature Len: %d\n", sigLen);

    /* Perform hash and sign to create signature */
    ret = wc_SignatureGenerate(
        hash_type, sig_type,
        fileBuf, fileLen,
        sigBuf, &sigLen,
        &rsaKey, sizeof(rsaKey),
        &rng);
    printf("Signature Generation: %s (%d)\n", (ret > 0) ? "Pass" : "Fail", ret);
    if(ret <= 0) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    printf("Signature Data:\n");
    hexdump(sigBuf, sigLen, 16);

#ifdef WOLFSSL_KEY_GEN
    /* Release and init new key */
    wc_FreeRsaKey(&rsaKey);
    wc_InitRsaKey(&rsaKey, NULL);

    /* Import the public key */
    ret = wc_RsaPublicKeyDecode(rsaPubKeyBuf, &idx, &rsaKey, rsaPubKeyLen);
    if (ret != 0) {
        printf("RSA public key import failed! %d\n", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }
#endif

    /* Perform signature verification */
    ret = wc_SignatureVerify(
        hash_type, sig_type,
        fileBuf, fileLen,
        sigBuf, sigLen,
        &rsaKey, sizeof(rsaKey));
    printf("Signature Verification: %s (%d)\n", (ret > 0) ? "Pass" : "Fail", ret);
    if(ret <= 0) {
        ret = EXIT_FAILURE;
    }

exit:
    /* Free */
#ifdef WOLFSSL_KEY_GEN
    if(rsaKeyBuf) {
        free(rsaKeyBuf);
    }
    if(rsaPubKeyBuf) {
        free(rsaPubKeyBuf);
    }
#endif
    if(sigBuf) {
        free(sigBuf);
    }
    wc_FreeRsaKey(&rsaKey);
    wc_FreeRng(&rng);

    return ret;
}

int main(int argc, char** argv)
{
    int ret = 0;
    int fileLen;
    byte* fileBuf = NULL;
    FILE* file = NULL;
    enum wc_SignatureType sig_type = WC_SIGNATURE_TYPE_ECC;
    enum wc_HashType hash_type = WC_HASH_TYPE_SHA256;

#if 0
    wolfSSL_Debugging_ON();
#endif

    /* Check arguments */
    if (argc < 2) {
        printf("Usage: signature <filename> <sig> <hash>\n");
        printf("  <sig>: 1=ECC (def), 2=RSA\n");
        printf("  <hash>: 1=MD2, 2=MD4, 3=MD5, 4=SHA, 5=SHA256 (def), 6=SHA384, 7=SHA512\n");
        return 1;
    }
    if(argc >= 3) {
        sig_type = atoi(argv[2]);
    }
    if(argc >= 4) {
        hash_type = atoi(argv[3]);
    }

    /* Verify hash type is supported */
    if (wc_HashGetDigestSize(hash_type) <= 0) {
        printf("Hash type %d not supported!\n", hash_type);
        return 1;
    }

    printf("Signature Example: Sig=%d, Hash=%d\n", sig_type, hash_type);

    /* Open file */
    file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("File %s does not exist!\n", argv[1]);
        ret = EXIT_FAILURE;
        goto exit;
    }

    /* Determine length of file */
    fseek(file, 0, SEEK_END);
    fileLen = (int) ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("File %s is %d bytes\n", argv[1], fileLen);

    /* Allocate buffer for image */
    fileBuf = malloc(fileLen);
    if(!fileBuf) {
        printf("File buffer malloc failed!\n");
        ret = EXIT_FAILURE;
        goto exit;
    }

    /* Load file into buffer */
    ret = (int)fread(fileBuf, 1, fileLen, file);
    if(ret != fileLen) {
        printf("Error reading file! %d", ret);
        ret = EXIT_FAILURE;
        goto exit;
    }

    /* Perform sign and verify */
    if (sig_type == WC_SIGNATURE_TYPE_ECC) {
        ret = ecc_sign_verify_test(hash_type, sig_type, fileBuf, fileLen);
    }
    else if (sig_type == WC_SIGNATURE_TYPE_RSA) {
        ret = rsa_sign_verify_test(hash_type, sig_type, fileBuf, fileLen);
    }
    else {
        ret = EXIT_FAILURE;
        printf("Signature type %d, not supported!\n", sig_type);
    }

exit:
    /* Free */
    if(fileBuf) {
        free(fileBuf);
    }
    if(file) {
        fclose(file);
    }

    return ret;
}