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
    #pragma mark IConversationThreadSystemMessage
    #pragma mark

    interaction IConversationThreadSystemMessage
    {
      enum SystemMessageTypes
      {
        SystemMessageType_NA,           // not a system message
        SystemMessageType_Unknown,      // unknown system message type

        SystemMessageType_CallPlaced,   // call was placed
        SystemMessageType_CallAnswered, // call was answered
        SystemMessageType_CallHungup,   // call was hung-up
      };

      static const char *toString(SystemMessageTypes type);
      static SystemMessageTypes toSystemMessageType(const char *type);

      //-----------------------------------------------------------------------
      // PURPOSE: Get the system message type
      // RETURNS: The system message mime type
      // NOTES:   All system messages use the same mime type
      static String getMessageType();

      //-----------------------------------------------------------------------
      // PURPOSE: Creates a system message related to the "call" system message
      //          types
      // RETURNS: JSON element to send as a message
      // NOTES:   Only the caller would create these kind of messages and
      //          send into the conversation thread.
      static ElementPtr createCallMessage(
                                          SystemMessageTypes type,
                                          IContactPtr callee,
                                          WORD errorCode = 0        // optional HTTP style error code (can cast as WORD from ICall::CallClosedReasons)
                                          );

      //-----------------------------------------------------------------------
      // PURPOSE: Helper routine to send the system message to the conversation
      //          thread.
      // NOTES:   Wraps the IConversationThread::sendMessage routine for
      //          sending convenience. Converts the JSON to a string and then
      //          sends the message using the system message type.
      static void sendMessage(
                              IConversationThreadPtr inRelatedConversationThread,
                              const char *messageID,
                              ElementPtr systemMessage,
                              bool signMessage
                              );

      //-----------------------------------------------------------------------
      // PURPOSE: Given a message and message type, attempt to parse as a
      //          system message.
      // RETURNS: SystemMessageType_NA - if message is not a system message
      //          SystemMessageType_Unknown - if message is a system message
      //                                      but is not understood
      //          outSystemMessage - JSON structure containing system message
      //                             information that needs parsing
      static SystemMessageTypes parseAsSystemMessage(
                                                     const char *inMessage,
                                                     const char *inMessageType,
                                                     ElementPtr &outSystemMessage
                                                     );

      //-----------------------------------------------------------------------
      // PURPOSE: Given a JSON system message extract the call information
      // RETURNS: outCallee - the callee contact
      //          outErrorCode - HTTP style error code
      static void getCallMessageInfo(
                                     IConversationThreadPtr inRelatedConversationThread,
                                     ElementPtr inSystemMessage,
                                     IContactPtr &outCallee,
                                     WORD &outErrorCode
                                     );

      virtual ~IConversationThreadSystemMessage() {}  // make polymophic
    };
  }
}
