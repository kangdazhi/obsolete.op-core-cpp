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

#pragma once

#include <openpeer/core/types.h>

namespace openpeer
{
  namespace core
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark interaction ISystemMessage
    #pragma mark

    interaction ISystemMessage
    {
      //-----------------------------------------------------------------------
      // PURPOSE: creates an empty system message that can be filled with data
      static ElementPtr createEmptySystemMessage();

      //-----------------------------------------------------------------------
      // PURPOSE: get the "messageType" to pass into
      //          "IConversationThread::sendMessage(...)"
      static String getMessageType();
    };


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark struct CallSystemMessage
    #pragma mark

    struct CallSystemMessage
    {
      enum CallSystemMessageTypes
      {
        CallSystemMessageType_None,     // not a call system message

        CallSystemMessageType_Placed,   // call was placed
        CallSystemMessageType_Answered, // call was answered
        CallSystemMessageType_Hungup,   // call was hung-up
      };

      static const char *toString(CallSystemMessageTypes type);
      static CallSystemMessageTypes toCallSystemMessageType(const char *type);

      //-----------------------------------------------------------------------
      // call system message data
      CallSystemMessageTypes mType;
      IContactPtr mCallee;
      WORD mErrorCode;

      //-----------------------------------------------------------------------
      // PURPOSE: constructor for new call system messages
      CallSystemMessage();
      CallSystemMessage(
                        CallSystemMessageTypes type,
                        IContactPtr callee,
                        WORD errorCode = 0
                        );
      CallSystemMessage(const CallSystemMessage &rValue);

      //-----------------------------------------------------------------------
      // PURPOSE: extract a call system message type from a system message
      // RETURNS: valid call system message or null CallSystemMessagePtr()
      //          if none is present.
      static CallSystemMessagePtr extract(
                                          ElementPtr dataEl,
                                          IAccountPtr account // callee is in context of account thus account is needed
                                          );

      //-----------------------------------------------------------------------
      // PURPOSE: insert call system information into a status message
      void insert(ElementPtr dataEl);

      //-----------------------------------------------------------------------
      // PURPOSE: check to see if the structure has any data
      bool hasData() const;

      //-----------------------------------------------------------------------
      // PURPOSE: returns debug information
      ElementPtr toDebug() const;
    };

  }
}
