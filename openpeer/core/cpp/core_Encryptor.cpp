  /*

 Copyright (c) 2014, Hookflash Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/core/internal/core_Encryptor.h>

#include <openpeer/services/IEncryptor.h>
#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <cryptopp/sha.h>

#include <zsLib/XML.h>

#include <errno.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using CryptoPP::SHA1;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark EncryptorData
      #pragma mark

      //-----------------------------------------------------------------------
      struct EncryptorData
      {
        SHA1 mHasher;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Encryptor
      #pragma mark

      //-----------------------------------------------------------------------
      Encryptor::Encryptor(
                           const char *inSourceFileName,
                           const char *inEncodingService
                           ) :
        mFile(NULL),
        mData(new EncryptorData)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inSourceFileName)

        mFile = fopen(inSourceFileName, "rb");
        if (mFile) {
          ZS_LOG_TRACE(log("encryptor file open") + ZS_PARAM("file name", inSourceFileName))
        } else {
          ZS_LOG_ERROR(Detail, log("file could not be open") + ZS_PARAM("file name", inSourceFileName))
        }

        mEncodingServiceName = String(inEncodingService);
        mKeyID = UseServicesHelper::randomString(20);
        mPassphrase = UseServicesHelper::randomString(32*8/5);
        mSalt = UseServicesHelper::randomString(16*8/5);

        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mPassphrase), mEncodingServiceName + ":" + mKeyID, UseServicesHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = UseServicesHelper::hash(mSalt, UseServicesHelper::HashAlgorthm_MD5);

        mEncryptor = UseEncryptor::create(*key, *iv);
        mBlockSize = mEncryptor->getOptimalBlockSize();
        if (mBlockSize < 1) {
          mBlockSize = 1;
        }

        size_t targetBlockSize = UseSettings::getUInt(OPENPEER_CORE_SETTING_ENCRYPT_BLOCK_SIZE);

        mBlockSize = (mBlockSize > targetBlockSize ? mBlockSize : ((targetBlockSize / mBlockSize) * mBlockSize));

        mBuffer = SecureByteBlockPtr(new SecureByteBlock(mBlockSize));
      }

      //-----------------------------------------------------------------------
      Encryptor::~Encryptor()
      {
        close();

        delete mData;
        mData = NULL;
      }

      //-----------------------------------------------------------------------
      EncryptorPtr Encryptor::create(
                                     const char *inSourceFileName,
                                     const char *inEncodingServiceName
                                     )
      {
        EncryptorPtr pThis(new Encryptor(inSourceFileName, inEncodingServiceName));
        if (!(pThis->mFile)) return EncryptorPtr();
        return pThis;
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Encryptor::encrypt()
      {
        SecureByteBlockPtr result = getNextBuffer();
        if (result) return result;

        if (!mFile) return SecureByteBlockPtr();

        size_t read = fread(*mBuffer, sizeof(BYTE), mBlockSize, mFile);

        if (0 != read) {
          mData->mHasher.Update(*mBuffer, read);
          appendBuffer(mEncryptor->encrypt(*mBuffer, read));
        }

        if (read == (sizeof(BYTE)*(mBlockSize))) return getNextBuffer();

        if (feof(mFile)) {
          appendBuffer(mEncryptor->finalize());
          ZS_LOG_TRACE(log("end of file reached"))
          return getNextBuffer();
        }

        int error = ferror(mFile);
        if (0 != error) {
          close(error);
          return getNextBuffer();
        }

        return getNextBuffer();
      }

      //-----------------------------------------------------------------------
      String Encryptor::finalize()
      {
        if (!mKeyID.hasData()) return String();

        SecureByteBlock originalDataHash(mData->mHasher.DigestSize());
        mData->mHasher.Final(originalDataHash);

        String hexHash = UseServicesHelper::convertToHex(originalDataHash);

        String proof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mPassphrase), "proof:" + mSalt + ":" + hexHash));

        String result = mEncodingServiceName
                        + ":" + UseServicesHelper::convertToBase64(OPENPEER_CORE_KEYING_ENCODING_TYPE)
                        + ":" + mKeyID
                        + ":" + mSalt
                        + ":" + proof
                        + ":" + mPassphrase;

        ZS_LOG_TRACE(log("final result") + ZS_PARAM("result",  result) + ZS_PARAM("hash", hexHash))

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Encryptor => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Encryptor::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Encryptor");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Encryptor::getNextBuffer()
      {
        if (mPendingBuffers.size() < 1) return SecureByteBlockPtr();

        SecureByteBlockPtr result = mPendingBuffers.front();
        mPendingBuffers.pop_front();
        return result;
      }

      //-----------------------------------------------------------------------
      void Encryptor::appendBuffer(SecureByteBlockPtr buffer)
      {
        if (!buffer) return;
        if (buffer->SizeInBytes() < 1) return;

        mPendingBuffers.push_back(buffer);
      }

      //-----------------------------------------------------------------------
      void Encryptor::close(int error)
      {
        if (!mFile) return;

        if (0 != error) {
          mKeyID.clear();
          ZS_LOG_ERROR(Detail, log("failed to read from file") + ZS_PARAM("error", error) + ZS_PARAM("error string", strerror(error)))
        }
        fclose(mFile);
        mFile = NULL;
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IEncryptor
    #pragma mark

    //-------------------------------------------------------------------------
    IEncryptorPtr IEncryptor::create(
                                     const char *inSourceFileName,
                                     const char *inEncodingServiceName
                                     )
    {
      return internal::Encryptor::create(inSourceFileName, inEncodingServiceName);
    }
  }
}
