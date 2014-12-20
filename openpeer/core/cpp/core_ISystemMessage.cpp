/*

 Copyright (c) 2013, SMB Phone Inc.
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

#include <openpeer/core/internal/types.h>

#include <openpeer/core/IConversationThread.h>
#include <openpeer/core/ISystemMessage.h>
#include <openpeer/core/IHelper.h>

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Numeric.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    using zsLib::Numeric;

    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::IMessageHelper, UseMessageHelper)
    ZS_DECLARE_TYPEDEF_PTR(internal::IContactForConversationThread, UseContact)

    namespace internal
    {
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::IConversationThreadSystemMessage");
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ISystemMessage
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr ISystemMessage::createEmptySystemMessage()
    {
      return Element::create(Definitions::Names::systemMessageRoot());
    }

    //-------------------------------------------------------------------------
    String ISystemMessage::getMessageType()
    {
      return Definitions::ValueKeywords::systemMessageType();
    }




    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark CallSystemMessage
    #pragma mark

    //-------------------------------------------------------------------------
    const char *CallSystemMessage::toString(CallSystemMessageStatuses type)
    {
      switch (type) {
        case CallSystemMessageStatus_None:      return "none";

        case CallSystemMessageStatus_Placed:    return Definitions::ValueKeywords::placed();
        case CallSystemMessageStatus_Answered:  return Definitions::ValueKeywords::answered();
        case CallSystemMessageStatus_Hungup:    return Definitions::ValueKeywords::answered();
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    CallSystemMessage::CallSystemMessageStatuses CallSystemMessage::toCallSystemMessageStatus(const char *inType)
    {
      static CallSystemMessageStatuses types[] =
      {
        CallSystemMessageStatus_Placed,
        CallSystemMessageStatus_Answered,
        CallSystemMessageStatus_Hungup,

        CallSystemMessageStatus_None
      };

      String type(inType);
      if (type.isEmpty()) return CallSystemMessageStatus_None;

      for (int index = 0; CallSystemMessageStatus_None != types[index]; ++index)
      {
        if (type == toString(types[index])) return types[index];
      }

      return CallSystemMessageStatus_None;
    }

    //-------------------------------------------------------------------------
    CallSystemMessage::CallSystemMessage() :
      mStatus(CallSystemMessageStatus_None),
      mErrorCode(0)
    {
    }

    //-------------------------------------------------------------------------
    CallSystemMessage::CallSystemMessage(
                                         CallSystemMessageStatuses status,
                                         const char *mediaType,
                                         const char *callID,
                                         IContactPtr callee,
                                         WORD errorCode
                                         ) :
      mStatus(status),
      mMediaType(mediaType),
      mCallID(callID),
      mCallee(callee),
      mErrorCode(errorCode)
    {
    }

    //-------------------------------------------------------------------------
    CallSystemMessage::CallSystemMessage(const CallSystemMessage &rValue) :
      mStatus(rValue.mStatus),
      mMediaType(rValue.mMediaType),
      mCallID(rValue.mCallID),
      mCallee(rValue.mCallee),
      mErrorCode(rValue.mErrorCode)
    {
    }

    //-------------------------------------------------------------------------
    CallSystemMessagePtr CallSystemMessage::extract(
                                                    ElementPtr dataEl,
                                                    IAccountPtr account
                                                    )
    {
      if (!dataEl) return CallSystemMessagePtr();

      ElementPtr callStatusEl = dataEl->findFirstChildElement(Definitions::Names::callStatusRoot());
      if (!callStatusEl) return CallSystemMessagePtr();

      String statusStr = UseMessageHelper::getElementText(callStatusEl->findFirstChildElement(Definitions::Names::status()));

      CallSystemMessageStatuses status = toCallSystemMessageStatus(statusStr);
      if (CallSystemMessageStatus_None == status) return CallSystemMessagePtr();

      CallSystemMessagePtr pThis(new CallSystemMessage);

      pThis->mStatus = status;

      pThis->mCallID = IMessageHelper::getAttributeID(callStatusEl);
      pThis->mMediaType = IMessageHelper::getElementTextAndDecode(callStatusEl->findFirstChildElement(Definitions::Names::mediaType()));
      String calleeStr = IMessageHelper::getElementTextAndDecode(callStatusEl->findFirstChildElement(Definitions::Names::callee()));
      String errorStr = IMessageHelper::getAttributeID(callStatusEl->findFirstChildElement(Definitions::Names::error()));

      try {
        pThis->mErrorCode = Numeric<WORD>(errorStr);
      } catch (Numeric<WORD>::ValueOutOfRange &) {
        ZS_LOG_WARNING(Detail, internal::slog("error value failed to convert") + ZS_PARAM("error", errorStr))
      }

      pThis->mCallee = internal::Contact::convert(UseContact::createFromPeerURI(internal::Account::convert(account), calleeStr));

      return pThis;
    }

    //-------------------------------------------------------------------------
    void CallSystemMessage::insert(ElementPtr dataEl)
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      if (CallSystemMessageStatus_None == mStatus) return;

      ElementPtr existingCallStatusEl = dataEl->findFirstChildElement(Definitions::Names::callStatusRoot());

      ElementPtr callStatuEl = IMessageHelper::createElementWithID(Definitions::Names::callStatusRoot(), mCallID);

      callStatuEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode(Definitions::Names::mediaType(), mMediaType));
      callStatuEl->adoptAsLastChild(UseMessageHelper::createElementWithText(Definitions::Names::status(), toString(mStatus)));
      callStatuEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode(Definitions::Names::callee(), mCallee->getPeerURI()));

      if (0 != mErrorCode) {
        callStatuEl->adoptAsLastChild(IMessageHelper::createElementWithID(Definitions::Names::error(), internal::string(mErrorCode)));
      }

      if (existingCallStatusEl) {
        existingCallStatusEl->adoptAsNextSibling(callStatuEl);
        existingCallStatusEl->orphan();
      } else {
        dataEl->adoptAsLastChild(callStatuEl);
      }
    }

    //-------------------------------------------------------------------------
    bool CallSystemMessage::hasData() const
    {
      return ((CallSystemMessageStatus_None != mStatus) ||
              (mCallID.hasData()) ||
              ((bool)mCallee) ||
              (0 != mErrorCode));
    }

    //-------------------------------------------------------------------------
    ElementPtr CallSystemMessage::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::CallSystemMessage");
      UseServicesHelper::debugAppend(resultEl, "status", toString(mStatus));
      UseServicesHelper::debugAppend(resultEl, "media type", mMediaType);
      UseServicesHelper::debugAppend(resultEl, "call id", mCallID);
      UseServicesHelper::debugAppend(resultEl, "calle", mCallee ? mCallee->getPeerURI() : String());
      UseServicesHelper::debugAppend(resultEl, "error", mErrorCode);
      return resultEl;
    }

  }
}
