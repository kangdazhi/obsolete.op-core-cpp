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


#include <openpeer/core/IConversationThread.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>


#define OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ROOT_NAME "meta"
#define OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ELEMENT_NAME "threadType"

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }

namespace openpeer
{
  namespace core
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ConversationThreadType
    #pragma mark

    //-------------------------------------------------------------------------
    const char *ConversationThreadType::toString(ConversationThreadTypes type)
    {
      switch (type) {
        case ConversationThreadType_None:         return "";
        case ConversationThreadType_ContactBased: return "contact";
        case ConversationThreadType_ThreadBased:  return "thread";
        case ConversationThreadType_RoomBased:    return "room";
      }

      return "";
    }
    
    //-------------------------------------------------------------------------
    ConversationThreadType::ConversationThreadTypes ConversationThreadType::toConversationThreadType(const char *inStr)
    {
      static const ConversationThreadTypes types[] =
      {
        ConversationThreadType_ContactBased,
        ConversationThreadType_ThreadBased,
        ConversationThreadType_RoomBased,
        ConversationThreadType_None,
      };
      
      String str(inStr);

      for (int loop = 0; types[loop] != ConversationThreadType_None; ++loop) {
        
        if (str == toString(types[loop]))
          return types[loop];
      }
      
      return ConversationThreadType_None;
    }
    
    //-------------------------------------------------------------------------
    ConversationThreadTypePtr ConversationThreadType::extract(ElementPtr converationThreadMetaDataEl)
    {
      ConversationThreadTypePtr type(new ConversationThreadType);
      if (!converationThreadMetaDataEl) return type;

      ElementPtr foundEl = converationThreadMetaDataEl->findFirstChildElement(OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ELEMENT_NAME);
      if (!foundEl) return type;

      String result = UseMessageHelper::getElementText(foundEl);
      
      type->mThreadType = ConversationThreadType::toConversationThreadType(result);

      return type;
    }

    //-------------------------------------------------------------------------
    void ConversationThreadType::insert(ElementPtr &converationThreadMetaDataEl) const
    {
      if (!converationThreadMetaDataEl) {
        converationThreadMetaDataEl = Element::create(OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ROOT_NAME);
      }

      ElementPtr insertEl = UseMessageHelper::createElementWithText(OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ELEMENT_NAME, toString(mThreadType));

      ElementPtr foundEl = converationThreadMetaDataEl->findFirstChildElement(OPENPEER_CORE_CONVERSATION_THREAD_TYPE_ELEMENT_NAME);
      if (!foundEl) {
        converationThreadMetaDataEl->adoptAsLastChild(insertEl);
        return;
      }

      foundEl->adoptAsNextSibling(insertEl);
      foundEl->orphan();
    }
    
    //-------------------------------------------------------------------------
    bool ConversationThreadType::hasData() const
    {
      return (ConversationThreadType_None != mThreadType);
    }
    
    //-------------------------------------------------------------------------
    ConversationThreadType::ConversationThreadType() :
      mThreadType(ConversationThreadType_None)
    {
    }

    //-------------------------------------------------------------------------
    ConversationThreadType::ConversationThreadType(const ConversationThreadType &op2) :
      mThreadType(op2.mThreadType)
    {
    }
    
    //-------------------------------------------------------------------------
    ElementPtr ConversationThreadType::toDebug() const
    {
      ElementPtr resultEl = Element::create("core::ConversationThreadType");

      UseServicesHelper::debugAppend(resultEl, "threadType", toString(mThreadType));

      return resultEl;
    }
  }
}
