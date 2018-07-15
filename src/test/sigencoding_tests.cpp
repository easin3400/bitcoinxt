// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_bitcoin.h"

#include "script/interpreter.h"
#include "script/sighashtype.h"

#include <boost/test/unit_test.hpp>

using valtype = std::vector<unsigned char>;

BOOST_FIXTURE_TEST_SUITE(sigencoding_tests, BasicTestingSetup)

static valtype SignatureWithHashType(valtype vchSig, SigHashType sigHash) {
    vchSig.push_back(static_cast<uint8_t>(sigHash));
    return vchSig;
}

static void CheckSignatureEncodingWithSigHashType(const valtype &vchSig,
                                                  uint32_t flags) {
    const bool hasForkId = (flags & SCRIPT_ENABLE_SIGHASH_FORKID) != 0;
    const bool hasStrictEnc = (flags & SCRIPT_VERIFY_STRICTENC) != 0;

    std::vector<SigHashType> allBaseTypes{
        SigHashType::ALL, SigHashType::NONE, SigHashType::SINGLE};

    std::vector<SigHashType> baseSigHashes;
    for (const SigHashType baseType : allBaseTypes) {
        baseSigHashes.push_back(baseType);
        baseSigHashes.push_back(baseType | SigHashType::ANYONECANPAY);
    }

    for (const SigHashType baseSigHash : baseSigHashes) {
        ScriptError err = SCRIPT_ERR_OK;

        // Check the signature with the proper forkid flag.
        SigHashType sigHash = baseSigHash;
        if (hasForkId) {
            sigHash |= SigHashType::FORKID;
        }
        valtype validSig = SignatureWithHashType(vchSig, sigHash);
        BOOST_CHECK(CheckSignatureEncoding(validSig, flags, &err));

        // If we have strict encoding, we prevent the use of undefined flags.
        std::array<SigHashType, 2> undefSigHashes{
            SigHashType(sigHash | SigHashType(0x20)),
            SigHashType::UNDEFINED
        };

        for (SigHashType undefSigHash : undefSigHashes) {
            valtype undefSighash = SignatureWithHashType(vchSig, undefSigHash);
            BOOST_CHECK_EQUAL(CheckSignatureEncoding(undefSighash, flags, &err),
                              !hasStrictEnc);
            if (hasStrictEnc) {
                BOOST_CHECK_EQUAL(err, SCRIPT_ERR_SIG_HASHTYPE);
            }
        }

        // If we check strict encoding, then invalid forkid is an error.
        SigHashType invalidSigHash = hasForkId
            ? baseSigHash & ~SigHashType::FORKID
            : baseSigHash | SigHashType::FORKID;
        valtype invalidSig = SignatureWithHashType(vchSig, invalidSigHash);

        BOOST_CHECK_EQUAL(CheckSignatureEncoding(invalidSig, flags, &err),
                          !hasStrictEnc);
        if (hasStrictEnc) {
            BOOST_CHECK_EQUAL(err,
                              hasForkId ? SCRIPT_ERR_MUST_USE_FORKID
                                        : SCRIPT_ERR_ILLEGAL_FORKID);
        }
    }
}

BOOST_AUTO_TEST_CASE(checksignatureencoding_test) {
    valtype nullfailsig{};
    valtype minimalsig{0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01};

    // If we add many more flags, this loop can get too expensive, but we can
    // rewrite in the future to randomly pick a set of flags to evaluate.
    for (uint32_t flags = 0; flags < (1U << 17); flags++) {
        ScriptError err = SCRIPT_ERR_OK;

        // Empty sig is always valid.
        BOOST_CHECK(CheckSignatureEncoding({}, flags, &err));

        // Signature are valid as long as the forkid flag is correct.
        CheckSignatureEncodingWithSigHashType(minimalsig, flags);
    }
}

BOOST_AUTO_TEST_CASE(checkpubkeyencoding_test) {
    valtype compressedKey0{0x02, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
                           0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
                           0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
                           0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    valtype compressedKey1{0x03, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
                           0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56,
                           0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78,
                           0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff};
    valtype fullKey{0x04, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
                    0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56,
                    0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
                    0xbc, 0xde, 0xf0, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
                    0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56,
                    0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
                    0xbc, 0xde, 0xf0, 0x0f, 0xff};

    std::vector<valtype> invalidKeys{
        // Degenerate keys.
        {},
        {0x00},
        {0x42},
        {0xff},
        // Invalid first byte 0x00.
        {0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Invalid first byte 0x01.
        {0x01, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Invalid first byte 0x05.
        {0x05, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0x05, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Invalid first byte 0xff.
        {0xff, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0xff, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Compressed key too short.
        {0x02, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0x03, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78,
         0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde,
         0xf0, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Compressed key too long.
        {0x02, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56,
         0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0xab, 0xba, 0x9a,
         0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
        {0x03, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0xab,
         0xba, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Compressed key, full key size.
        {0x02, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        {0x03, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Full key, too short.
        {0x04, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
         0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Full key, too long.
        {0x04, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34,
         0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a,
         0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
         0x56, 0x78, 0xab, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78,
         0x9a, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xde,
         0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x0f, 0xff},
        // Full key, compressed key size.
        {0x04, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56,
         0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0xab, 0xba, 0x9a,
         0xde, 0xf0, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0},
    };

    // If we add many more flags, this loop can get too expensive, but we can
    // rewrite in the future to randomly pick a set of flags to evaluate.
    for (uint32_t flags = 0; flags < (1U << 17); flags++) {
        ScriptError err = SCRIPT_ERR_OK;

        // Compressed pubkeys are always valid.
        BOOST_CHECK(CheckPubKeyEncoding(compressedKey0, flags, &err));
        BOOST_CHECK(CheckPubKeyEncoding(compressedKey1, flags, &err));

        // If SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE is specified, full key are
        // disabled.
        const bool allowFullKey =
            (flags & SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE) == 0;
        BOOST_CHECK_EQUAL(CheckPubKeyEncoding(fullKey, flags, &err),
                          allowFullKey);
        if (!allowFullKey) {
            BOOST_CHECK_EQUAL(err, SCRIPT_ERR_NONCOMPRESSED_PUBKEY);
        }

        // If SCRIPT_VERIFY_STRICTENC or SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE is
        // specified, we rule out invalid keys.
        const bool hasStrictEnc = (flags & SCRIPT_VERIFY_STRICTENC) != 0;
        const bool allowInvalidKeys = allowFullKey && !hasStrictEnc;
        for (const valtype &key : invalidKeys) {
            BOOST_CHECK_EQUAL(CheckPubKeyEncoding(key, flags, &err),
                              allowInvalidKeys);
            if (!allowInvalidKeys) {
                BOOST_CHECK_EQUAL(err,
                                  hasStrictEnc
                                      ? SCRIPT_ERR_PUBKEYTYPE
                                      : SCRIPT_ERR_NONCOMPRESSED_PUBKEY);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
