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

//#include <openpeer/core/IConversationThread.h>
#include <openpeer/core/IConversationThreadComposingStatus.h>
//#include <openpeer/core/IHelper.h>
//
//#include <openpeer/core/internal/core_Account.h>
//#include <openpeer/core/internal/core_Contact.h>
//
#include <openpeer/stack/message/IMessageHelper.h>
//
//#include <openpeer/services/IHelper.h>
//
//#include <zsLib/Numeric.h>
//#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

#define OPENPEEER_CORE_CONVERSATION_THREAD_SYSTEM_MESSAGE_TYPE "text/json-system-message"

namespace openpeer
{
  namespace core
  {
//    using zsLib::Numeric;
//
    ZS_DECLARE_USING_PTR(openpeer::stack::message, IMessageHelper)
//    ZS_DECLARE_TYPEDEF_PTR(internal::IContactForConversationThread, UseContact)

    namespace internal
    {
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::IConversationThreadComposingStatus");
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
    const char *IConversationThreadComposingStatus::toString(ComposingStates state)
    {
      switch (state) {
        case ComposingState_None:             return "none";

        case ComposingState_Inactive:         return "inactive";
        case ComposingState_Active:           return "active";
        case ComposingState_Gone:             return "gone";
        case ComposingState_Composing:        return "composing";
        case ComposingState_Paused:           return "paused";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IConversationThreadComposingStatus::ComposingStates IConversationThreadComposingStatus::toComposingState(const char *inState)
    {
      static ComposingStates states[] =
      {
        ComposingState_Inactive,
        ComposingState_Active,
        ComposingState_Gone,
        ComposingState_Composing,
        ComposingState_Paused,

        ComposingState_None
      };

      String state(inState);
      if (state.isEmpty()) return ComposingState_None;

      for (int index = 0; ComposingState_None != states[index]; ++index)
      {
        if (state == toString(states[index])) return states[index];
      }

      return ComposingState_None;
    }

    //-------------------------------------------------------------------------
    void IConversationThreadComposingStatus::updateComposingStatus(
                                                                   ElementPtr &ioContactStatusInThreadEl,
                                                                   ComposingStates composing
                                                                   )
    {
      if (!ioContactStatusInThreadEl) {
        if ((ComposingState_None == composing) ||
            (ComposingState_Inactive == composing)) {
          ZS_LOG_TRACE(internal::slog("default state applies and empty status JSON object thus return nothing"))
          return;
        }

        ioContactStatusInThreadEl = Element::create("status");
      }

      ElementPtr newComposingEl = IMessageHelper::createElementWithText("composing", toString(composing));

      ElementPtr oldComposingEl = ioContactStatusInThreadEl->findFirstChildElement("composing");
      if (oldComposingEl) {
        if (newComposingEl)
          oldComposingEl->adoptAsPreviousSibling(newComposingEl);
        oldComposingEl->orphan();
      } else {
        ioContactStatusInThreadEl->adoptAsLastChild(newComposingEl);
      }
    }

    //-------------------------------------------------------------------------
    IConversationThreadComposingStatus::ComposingStates IConversationThreadComposingStatus::getComposingStatus(ElementPtr contactStatusInThreadEl)
    {
      if (!contactStatusInThreadEl) return ComposingState_None;
      String composingStr = IMessageHelper::getElementText(contactStatusInThreadEl->findFirstChildElement("composing"));
      return toComposingState(composingStr);
    }


  }
}
