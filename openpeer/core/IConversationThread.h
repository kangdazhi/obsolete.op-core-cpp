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
    #pragma mark IConversationThread
    #pragma mark

    interaction IConversationThread
    {
      enum MessageDeliveryStates
      {
        MessageDeliveryState_Discovering      = 0,
        MessageDeliveryState_UserNotAvailable = 1,
        MessageDeliveryState_Sent             = 2,
        MessageDeliveryState_Delivered        = 3,
        MessageDeliveryState_Read             = 4,
      };

      enum ContactConnectionStates
      {
        ContactConnectionState_NotApplicable,

        ContactConnectionState_Finding,
        ContactConnectionState_Connected,
        ContactConnectionState_Disconnected
      };

      static const char *toString(MessageDeliveryStates state);
      static const char *toString(ContactConnectionStates state);

      static ElementPtr toDebug(IConversationThreadPtr thread);

      struct Definitions
      {
        struct Names {
          //-------------------------------------------------------------------
          // PURPOSE: Returns the "name" part for the json meta data object
          //          as contained in JSON blob {"metaData" : {...} }
          static const char *metaDataName()                                     {return "metaData";}
        };
      };


      //-----------------------------------------------------------------------
      // PURPOSE: Create a new conversation thread with only "self" as the
      //          current contact.
      static IConversationThreadPtr create(
                                           IAccountPtr account,
                                           const IdentityContactList &identityContactsOfSelf,                     // filter only the identities that are desired for anyone who joins the conversation thread to know
                                           const ContactProfileInfoList &addContacts = ContactProfileInfoList(),  // add these contacts by default to the conversation (can be empty)
                                           const char *threadID = NULL,                                           // optional thread ID to use for new thread
                                           ElementPtr metaData = ElementPtr()                                     // optional meta data to assign to the thread
                                           );

      //-----------------------------------------------------------------------
      // PURPOSE: Gets a list of the current conversation threads active on an
      //          account.
      static ConversationThreadListPtr getConversationThreads(IAccountPtr account);

      //-----------------------------------------------------------------------
      // PURPOSE: Given a conversation thread ID return the conversation thread
      //          object.
      static IConversationThreadPtr getConversationThreadByID(
                                                              IAccountPtr account,
                                                              const char *threadID
                                                              );

      //-----------------------------------------------------------------------
      // PURPOSE: Get a local ID representing the conversation thread
      // NOTES:   Each contact within the conversation thread may have a
      //          different "local" conversation thread ID.
      //          Guarenteeed to be unique from any other conversation thread.
      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the thread ID for the current conversation thread.
      // NOTES:   All contacts within the same conversation thread will have
      //          the same thread ID.
      virtual String getThreadID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the thread ID for the current conversation thread.
      // NOTES:   OpenPeer handles conversation threads by always designating
      //          only contact as the host. The host can change dynamically
      //          as contacts come and go online/offline.
      virtual bool amIHost() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the instance of the account associated to the
      //          conversation thread.
      // RETURNS: The associated account object.
      virtual IAccountPtr getAssociatedAccount() const = 0;
      
      //-----------------------------------------------------------------------
      // PURPOSE: Get the meta data associated with a converation thread
      // RETURNS: The associated meta data or ElementPtr if none
      virtual ElementPtr getMetaData() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the list of contacts currently in the conversation
      //          thread.
      virtual ContactListPtr getContacts() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Adds other contacts to the conversation thread and includes
      //          a list of identities to include for each contact that
      //          will be readable by all other contacts in the conversation
      //          thread.
      virtual void addContacts(const ContactProfileInfoList &contactProfileInfos) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Removes a set of contacts from the conversation thread.
      virtual void removeContacts(const ContactList &contacts) = 0;


      
      //-----------------------------------------------------------------------
      // PURPOSE: Get the list of identities reported associated to a contact
      //          in the conversation thread.
      // NOTES:   Identities returned may only be a subset of the identities
      //          of the contact due to privacy concerns.
      virtual IdentityContactListPtr getIdentityContactList(IContactPtr contact) const = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: Get the connection state of a contact in the conversation
      //          thread.
      virtual ContactConnectionStates getContactConnectionState(IContactPtr contact) const = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: Create an empty JSON status blob ready to fill with
      //          additional structure data
      // NOTES:   Use "ComposingStatus" to insert composing status information
      //          into this JSON blob.
      static ElementPtr createEmptyStatus();


      //-----------------------------------------------------------------------
      // PURPOSE: Get the status of a contact in the conversation thread.
      // NOTES:   Use "ComposingStatus" structure to extract information out
      //          of the composing structure.
      virtual ElementPtr getContactStatus(IContactPtr contact) const = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: Set the status of yourself in the conversation thread
      // NOTES:   Use "createEmptyStatus" to construct an empty status
      //          and use "ComposingStatus" structure to fill with information.
      virtual void setStatusInThread(ElementPtr contactStatusInThreadOfSelf) = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: Send a message to all the other contacts in the conversation
      //          thread.
      virtual void sendMessage(
                               const char *messageID,
                               const char *replacesMessageID,
                               const char *messageType,
                               const char *message,
                               bool signMessage
                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Gets information about a message that has been received.
      // RETURNS: false if the message ID is not known
      // NOTES:   outReplacesMessageID - care should be made to only accept
      //                                 a replacement for an existing message
      //                                 ID if the same user composed the
      //                                 message previously (otherwise it's
      //                                 illegal).
      //          outValidated - if replacing an existing message that was
      //                         previously validated the new message should be
      //                         validated as well or it should be considered
      //                         illegal.
      virtual bool getMessage(
                              const char *messageID,
                              String &outReplacesMessageID,   // if this new message replaces an existing message
                              IContactPtr &outFrom,
                              String &outMessageType,
                              String &outMessage,
                              Time &outTime,
                              bool &outValidated              // message was validated as having been composed by the "from" contact
                              ) const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Gets the current delivery state of a message by its message
      //          ID
      virtual bool getMessageDeliveryState(
                                           const char *messageID,
                                           MessageDeliveryStates &outDeliveryState
                                           ) const = 0;
      
      //-----------------------------------------------------------------------
      // PURPOSE: Sets the current delivery state of a message by its message
      //          ID.
      // NOTE:    A message cannot go to a lessor delivery state. If done the
      //          delivery state will be set to the
      virtual void setMessageDeliveryState(
                                           const char *inMessageID,
                                           MessageDeliveryStates inDeliveryState
                                           ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: mark all received messages thus far as read
      virtual void markAllMessagesRead() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IConversationThreadDelegate
    #pragma mark

    interaction IConversationThreadDelegate
    {
      typedef IConversationThread::MessageDeliveryStates MessageDeliveryStates;
      typedef IConversationThread::ContactConnectionStates ContactConnectionStates;

      virtual void onConversationThreadNew(IConversationThreadPtr conversationThread) = 0;

      virtual void onConversationThreadContactsChanged(IConversationThreadPtr conversationThread) = 0;
      virtual void onConversationThreadContactConnectionStateChanged(
                                                                     IConversationThreadPtr conversationThread,
                                                                     IContactPtr contact,
                                                                     ContactConnectionStates state
                                                                     ) = 0;
      virtual void onConversationThreadContactStatusChanged(
                                                            IConversationThreadPtr conversationThread,
                                                            IContactPtr contact
                                                            ) = 0;

      virtual void onConversationThreadMessage(
                                               IConversationThreadPtr conversationThread,
                                               const char *messageID
                                               ) = 0;

      virtual void onConversationThreadMessageDeliveryStateChanged(
                                                                   IConversationThreadPtr conversationThread,
                                                                   const char *messageID,
                                                                   MessageDeliveryStates state
                                                                   ) = 0;

      virtual void onConversationThreadPushMessage(
                                                   IConversationThreadPtr conversationThread,
                                                   const char *messageID,
                                                   IContactPtr contact
                                                   ) = 0;
    };
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ConversationThreadType
    #pragma mark
    
    struct ConversationThreadType
    {
      enum ConversationThreadTypes
      {
        ConversationThreadType_None,

        ConversationThreadType_ContactBased,
        ConversationThreadType_ThreadBased,
        ConversationThreadType_RoomBased,
      };
      
      static const char *toString(ConversationThreadTypes type);
      static ConversationThreadTypes toConversationThreadType(const char *str);

      struct Definitions
      {
        struct Names {
          //-------------------------------------------------------------------
          // PURPOSE: Returns the "name" part for the json meta data object
          //          as contained in JSON blob
          //          {"metaData" : { "conversationType" : {...} } }
          static const char *conversationType()                                 {return "conversationType";}
        };
        
        //---------------------------------------------------------------------
        // PURPOSE: Return the value keywords for use within meta data json
        //          for converstation thread types
        struct ValueKeywords {
          static const char *contactBased()                                     {return "contact";}
          static const char *threadBased()                                      {return "thread";}
          static const char *roomBased()                                        {return "room";}
        };
      };
      

      ConversationThreadTypes mThreadType;
      
      static ConversationThreadTypePtr extract(ElementPtr converationThreadMetaDataEl);
      void insert(ElementPtr &converationThreadMetaDataEl) const;
      
      bool hasData() const;
      
      ConversationThreadType();
      ConversationThreadType(const ConversationThreadType &op2);

      ElementPtr toDebug() const;
    };

  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::core::IConversationThreadDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IConversationThread::MessageDeliveryStates, MessageDeliveryStates)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IConversationThreadPtr, IConversationThreadPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IContactPtr, IContactPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::core::IConversationThread::ContactConnectionStates, ContactConnectionStates)
ZS_DECLARE_PROXY_METHOD_1(onConversationThreadNew, IConversationThreadPtr)
ZS_DECLARE_PROXY_METHOD_1(onConversationThreadContactsChanged, IConversationThreadPtr)
ZS_DECLARE_PROXY_METHOD_3(onConversationThreadContactConnectionStateChanged, IConversationThreadPtr, IContactPtr, ContactConnectionStates)
ZS_DECLARE_PROXY_METHOD_2(onConversationThreadContactStatusChanged, IConversationThreadPtr, IContactPtr)
ZS_DECLARE_PROXY_METHOD_2(onConversationThreadMessage, IConversationThreadPtr, const char *)
ZS_DECLARE_PROXY_METHOD_3(onConversationThreadMessageDeliveryStateChanged, IConversationThreadPtr, const char *, MessageDeliveryStates)
ZS_DECLARE_PROXY_METHOD_3(onConversationThreadPushMessage, IConversationThreadPtr, const char *, IContactPtr)
ZS_DECLARE_PROXY_END()
