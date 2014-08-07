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
#include <openpeer/core/IConversationThreadSystemMessage.h>
#include <openpeer/core/IHelper.h>

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Contact.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Numeric.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

#define OPENPEEER_CORE_CONVERSATION_THREAD_SYSTEM_MESSAGE_TYPE "text/json-system-message"

namespace openpeer
{
  namespace core
  {
    using zsLib::Numeric;

    ZS_DECLARE_USING_PTR(openpeer::stack::message, IMessageHelper)
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
    #pragma mark IConversationThread
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IConversationThreadSystemMessage::toString(SystemMessageTypes type)
    {
      switch (type) {
        case SystemMessageType_NA:            return "na";
        case SystemMessageType_Unknown:       return "unknown";

        case SystemMessageType_CallPlaced:    return "call_placed";
        case SystemMessageType_CallAnswered:  return "call_answered";
        case SystemMessageType_CallHungup:    return "call_hungup";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IConversationThreadSystemMessage::SystemMessageTypes IConversationThreadSystemMessage::toSystemMessageType(const char *inType)
    {
      static SystemMessageTypes types[] =
      {
        SystemMessageType_CallPlaced,
        SystemMessageType_CallAnswered,
        SystemMessageType_CallHungup,

        SystemMessageType_NA
      };

      String type(inType);
      if (type.isEmpty()) return SystemMessageType_NA;

      for (int index = 0; SystemMessageType_NA != types[index]; ++index)
      {
        if (type == toString(types[index])) return types[index];
      }

      return SystemMessageType_Unknown;
    }

    //-------------------------------------------------------------------------
    String IConversationThreadSystemMessage::getMessageType()
    {
      return OPENPEEER_CORE_CONVERSATION_THREAD_SYSTEM_MESSAGE_TYPE;
    }

    //-------------------------------------------------------------------------
    ElementPtr IConversationThreadSystemMessage::createCallMessage(
                                                                   SystemMessageTypes type,
                                                                   IContactPtr callee,
                                                                   WORD errorCode
                                                                   )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!callee)

      ElementPtr messageEl = Element::create("system");

      switch (type) {
        case SystemMessageType_NA:            ZS_THROW_INVALID_ARGUMENT("illegal type")
        case SystemMessageType_Unknown:       ZS_THROW_INVALID_ARGUMENT("illegal type")

        case SystemMessageType_CallPlaced:
        case SystemMessageType_CallAnswered:
        case SystemMessageType_CallHungup:    break;
      }

      messageEl->adoptAsLastChild(IMessageHelper::createElementWithText("type", toString(type)));
      messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("callee", callee->getPeerURI()));

      if (0 != errorCode) {
        messageEl->adoptAsLastChild(IMessageHelper::createElementWithID("error", internal::string(errorCode)));
      }

      return messageEl;
    }

    //-------------------------------------------------------------------------
    void IConversationThreadSystemMessage::sendMessage(
                                                       IConversationThreadPtr inRelatedConversationThread,
                                                       const char *messageID,
                                                       ElementPtr systemMessage,
                                                       bool signMessage
                                                       )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!inRelatedConversationThread)
      ZS_THROW_INVALID_ARGUMENT_IF(!systemMessage)

      String message = IHelper::convertToString(systemMessage);

      inRelatedConversationThread->sendMessage(messageID, NULL, getMessageType(), message, signMessage);
    }

    //-------------------------------------------------------------------------
    IConversationThreadSystemMessage::SystemMessageTypes IConversationThreadSystemMessage::parseAsSystemMessage(
                                                                                                                const char *inMessage,
                                                                                                                const char *inMessageType,
                                                                                                                ElementPtr &outSystemMessage
                                                                                                                )
    {
      String message(inMessage);
      String messageType(inMessageType);

      if (messageType != getMessageType()) return SystemMessageType_NA;

      ElementPtr messageEl = IHelper::createElement(message);

      String typeStr = IMessageHelper::getElementText(messageEl->findFirstChildElement("type"));

      SystemMessageTypes type = toSystemMessageType(typeStr);
      if ((SystemMessageType_NA == type) ||
          (SystemMessageType_Unknown == type)) return type;

      outSystemMessage = messageEl;

      return type;
    }

    //-------------------------------------------------------------------------
    void IConversationThreadSystemMessage::getCallMessageInfo(
                                                              IConversationThreadPtr inRelatedConversationThread,
                                                              ElementPtr inSystemMessage,
                                                              IContactPtr &outCallee,
                                                              WORD &outErrorCode
                                                              )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!inRelatedConversationThread)
      ZS_THROW_INVALID_ARGUMENT_IF(!inSystemMessage)

      outCallee = IContactPtr();
      outErrorCode = 0;

      String calleeStr = IMessageHelper::getElementTextAndDecode(inSystemMessage->findFirstChildElement("callee"));
      String errorStr = IMessageHelper::getAttributeID(inSystemMessage->findFirstChildElement("error"));

      if (errorStr.isEmpty()) return;

      try {
        outErrorCode = Numeric<WORD>(errorStr);
      } catch (Numeric<WORD>::ValueOutOfRange &) {
        ZS_LOG_WARNING(Detail, internal::slog("error value failed to convert") + ZS_PARAM("error", errorStr))
      }

      IAccountPtr account = inRelatedConversationThread->getAssociatedAccount();
      if (!account) {
        ZS_LOG_WARNING(Detail, internal::slog("account is gone thus cannot construct contact"))
        return;
      }
      outCallee = internal::Contact::convert(UseContact::createFromPeerURI(internal::Account::convert(account), calleeStr));
    }
  }
}
