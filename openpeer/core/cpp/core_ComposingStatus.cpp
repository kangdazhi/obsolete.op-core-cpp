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

#include <openpeer/core/ComposingStatus.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

#define OPENPEEER_CORE_CONVERSATION_THREAD_SYSTEM_MESSAGE_TYPE "text/json-system-message"

namespace openpeer
{
  namespace core
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::IMessageHelper, UseMessageHelper)

    namespace internal
    {
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "core::ComposingStatus");
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ComposingStatus
    #pragma mark

    //-------------------------------------------------------------------------
    const char *ComposingStatus::toString(ComposingStates state)
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
    ComposingStatus::ComposingStates ComposingStatus::toComposingState(const char *inState)
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
    ComposingStatus::ComposingStatus() :
      mComposingStatus(ComposingState_None)
    {
    }

    //-------------------------------------------------------------------------
    ComposingStatus::ComposingStatus(ComposingStates state) :
      mComposingStatus(state)
    {
    }

    //-------------------------------------------------------------------------
    ComposingStatus::ComposingStatus(const ComposingStatus &rValue) :
      mComposingStatus(rValue.mComposingStatus)
    {
    }

    //-------------------------------------------------------------------------
    ComposingStatusPtr ComposingStatus::extract(ElementPtr dataEl)
    {
      if (!dataEl) return ComposingStatusPtr();

      String composingStr = UseMessageHelper::getElementText(dataEl->findFirstChildElement("composing"));

      ComposingStates state = toComposingState(composingStr);
      if (ComposingState_None == state) return ComposingStatusPtr();

      return ComposingStatusPtr(new ComposingStatus(state));
    }

    //-------------------------------------------------------------------------
    void ComposingStatus::insert(ElementPtr dataEl)
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

      if (ComposingState_None == mComposingStatus) return;

      ElementPtr existingComposingEl = dataEl->findFirstChildElement("composing");

      ElementPtr composingEl = UseMessageHelper::createElementWithText("composing", toString(mComposingStatus));

      if (existingComposingEl) {
        existingComposingEl->adoptAsNextSibling(composingEl);
        existingComposingEl->orphan();
      } else {
        dataEl->adoptAsLastChild(composingEl);
      }
    }

    //-------------------------------------------------------------------------
    bool ComposingStatus::hasData() const
    {
      return (ComposingState_None != mComposingStatus);
    }

    //-------------------------------------------------------------------------
    ElementPtr ComposingStatus::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::ComposingStatus");
      UseServicesHelper::debugAppend(resultEl, "composing state", toString(mComposingStatus));
      return resultEl;
    }

  }
}
