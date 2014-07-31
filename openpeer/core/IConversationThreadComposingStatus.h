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
    #pragma mark IConversationThreadComposingStatus
    #pragma mark

    interaction IConversationThreadComposingStatus
    {
      enum ComposingStates
      {
        ComposingState_None,      // contact has no composing status

        ComposingState_Inactive,  // contact is not actively participating in conversation (assumed default if "none")
        ComposingState_Active,    // contact is active in the conversation
        ComposingState_Gone,      // contact is effectively gone from conversation
        ComposingState_Composing, // contact is composing a message
        ComposingState_Paused,    // contact was composing a message but is no longer composing
      };

      static const char *toString(ComposingStates state);
      static ComposingStates toComposingState(const char *state);

      //-----------------------------------------------------------------------
      // PURPOSE: Creates or updates a "contactStatus" in conversation thread
      //          JSON information blob.
      // RETURNS: ioContactStatusInThreadEl- the existing contact status JSON
      //                                     blob is edited in place and/or a
      //                                     new JSON blob is created
      // NOTES:   If ioContactStatusInThreadEl is empty and the state passed
      //          in is "ComposingState_None" or "ComposingState_Inactive" the
      //          ioContactStatusInThreadEl will continue to be set to
      //          ElementPtr() without a new JSON blob being constructed.
      static void updateComposingStatus(
                                        ElementPtr &ioContactStatusInThreadEl, // value can start as ElementPtr() and will be automatically filled in
                                        ComposingStates composing              // the new composing state
                                        );

      //-----------------------------------------------------------------------
      // PURPOSE: Given a "contactStatus" in conversation thread JSON
      //          information blob, extract out the composing state (if any)
      // RETURNS: "ComposingState_None" when there is no composing state or the
      //          current composing state.
      static ComposingStates getComposingStatus(ElementPtr contactStatusInThreadEl);

      virtual ~IConversationThreadComposingStatus() {}  // make polymophic
    };

  }
}
