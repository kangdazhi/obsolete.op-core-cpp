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

#include <openpeer/core/internal/core_Account_DelegateFilter.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>

#include <zsLib/XML.h>

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => DelegateFilter
      #pragma mark

      //-----------------------------------------------------------------------
      Account::DelegateFilter::DelegateFilter(
                                              AccountPtr outer,
                                              IConversationThreadDelegatePtr conversationThreadDelegate,
                                              ICallDelegatePtr callDelegate
                                              ) :
        MessageQueueAssociator(outer->getAssociatedMessageQueue()),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mConversationThreadDelegate(conversationThreadDelegate),
        mCallDelegate(callDelegate)
      {
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::init()
      {
      }

      //-----------------------------------------------------------------------
      Account::DelegateFilter::~DelegateFilter()
      {
        ZS_LOG_DEBUG(log("destructor called"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::DelegateFilter::toDebug(DelegateFilterPtr filter)
      {
        if (!filter) return ElementPtr();
        return filter->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::DelegateFilter => friend ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      Account::DelegateFilterPtr Account::DelegateFilter::create(
                                                                 AccountPtr outer,
                                                                 IConversationThreadDelegatePtr conversationThreadDelegate,
                                                                 ICallDelegatePtr callDelegate
                                                                 )
      {
        DelegateFilterPtr pThis(new DelegateFilter(outer, conversationThreadDelegate, callDelegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::DelegateFilter => IConversationThreadDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadNew(IConversationThreadPtr conversationThread)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("delaying conversation thread new event") + ZS_PARAM("thread", conversationThread->getID()))

        FilteredEvents event;

        event.mConversationThread = conversationThread;

        mFilteredEvents[conversationThread->getID()] = event;
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadContactsChanged(IConversationThreadPtr conversationThread)
      {
        AutoRecursiveLock lock(*this);

        if (!mConversationThreadDelegate) return;

        FilteredEvents *event = find(conversationThread->getID());
        if (event) {
          ZS_LOG_DEBUG(log("delaying conversation contacts changed event") + ZS_PARAM("thread", conversationThread->getID()))

          get(event->mFiredContactsChanged) = true;
          return;
        }

        ZS_LOG_DEBUG(log("firing conversation contacts changed event now") + ZS_PARAM("thread", conversationThread->getID()))

        try {
          mConversationThreadDelegate->onConversationThreadContactsChanged(conversationThread);
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadContactStateChanged(
                                                                            IConversationThreadPtr conversationThread,
                                                                            IContactPtr contact,
                                                                            ContactStates state
                                                                            )
      {
        AutoRecursiveLock lock(*this);

        if (!mConversationThreadDelegate) return;

        FilteredEvents *event = find(conversationThread->getID());
        if (event) {
          ZS_LOG_TRACE(log("delaying contact change changed event") + ZS_PARAM("thread", conversationThread->getID()) + ZS_PARAM("contact", contact->getID()) + ZS_PARAM("state", IConversationThread::toString(state)))

          event->mFiredContactStateChanges.push_back(ContactStateChangedEvent(contact, state));
          return;
        }

        ZS_LOG_TRACE(log("firing contact change changed event now") + ZS_PARAM("thread", conversationThread->getID()) + ZS_PARAM("contact", contact->getID()) + ZS_PARAM("state", IConversationThread::toString(state)))

        try {
          mConversationThreadDelegate->onConversationThreadContactsChanged(conversationThread);
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadMessage(
                                                                IConversationThreadPtr conversationThread,
                                                                const char *messageID
                                                                )
      {
        AutoRecursiveLock lock(*this);

        if (!mConversationThreadDelegate) return;

        ZS_LOG_TRACE(log("firing conversation thread message") + ZS_PARAM("thread", conversationThread->getID()) + ZS_PARAM("message ID", messageID))

        fireNow(conversationThread->getID());

        try {
          mConversationThreadDelegate->onConversationThreadMessage(conversationThread, messageID);
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadMessageDeliveryStateChanged(
                                                                                    IConversationThreadPtr conversationThread,
                                                                                    const char *messageID,
                                                                                    MessageDeliveryStates state
                                                                                    )
      {
        AutoRecursiveLock lock(*this);

        if (!mConversationThreadDelegate) return;

        ZS_LOG_TRACE(log("firing conversation thread message delivery state changed") + ZS_PARAM("thread", conversationThread->getID()) + ZS_PARAM("message ID", messageID) + ZS_PARAM("state", IConversationThread::toString(state)))

        fireNow(conversationThread->getID());

        try {
          mConversationThreadDelegate->onConversationThreadMessageDeliveryStateChanged(conversationThread, messageID, state);
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onConversationThreadPushMessage(
                                                                    IConversationThreadPtr conversationThread,
                                                                    const char *messageID,
                                                                    IContactPtr contact
                                                                    )
      {
        AutoRecursiveLock lock(*this);

        if (!mConversationThreadDelegate) return;

        ZS_LOG_TRACE(log("firing conversation thread push message") + ZS_PARAM("thread", conversationThread->getID()) + ZS_PARAM("contact", contact->getID()))

        fireNow(conversationThread->getID());

        try {
          mConversationThreadDelegate->onConversationThreadPushMessage(conversationThread, messageID, contact);
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::DelegateFilter => ICallDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::onCallStateChanged(
                                                       ICallPtr call,
                                                       CallStates state
                                                       )
      {
        AutoRecursiveLock lock(*this);

        if (!mCallDelegate) return;

        ZS_LOG_TRACE(log("call state changed") + ZS_PARAM("call", call->getID()) + ZS_PARAM("state", ICall::toString(state)))

        IConversationThreadPtr conversationThread = call->getConversationThread();
        if (conversationThread) {
          fireNow(conversationThread->getID());
        }

        try {
          mCallDelegate->onCallStateChanged(call, state);
        } catch(ICallDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::DelegateFilter => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Account::DelegateFilter::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("core::Account::DelegateFilter");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::DelegateFilter::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("core::Account::DelegateFilter");

        IHelper::debugAppend(resultEl, "id", mID);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::cancel()
      {
        AutoRecursiveLock lock(*this);
      }

      //-----------------------------------------------------------------------
      Account::DelegateFilter::FilteredEvents *Account::DelegateFilter::find(PUID conversationThreadID)
      {
        FilteredEventMap::iterator found = mFilteredEvents.find(conversationThreadID);
        if (mFilteredEvents.end() == found) return NULL;

        FilteredEvents &event = (*found).second;

        return &event;
      }

      //-----------------------------------------------------------------------
      void Account::DelegateFilter::fireNow(PUID conversationThreadID)
      {
        FilteredEventMap::iterator found = mFilteredEvents.find(conversationThreadID);
        if (mFilteredEvents.end() == found) {
          ZS_LOG_TRACE(log("nothing in backlog to fire"))
          return;
        }

        FilteredEvents &event = (*found).second;

        ZS_LOG_DEBUG(log("firing previously delayed new conversation now") + ZS_PARAM("thread", event.mConversationThread->getID()))

        try {
          mConversationThreadDelegate->onConversationThreadNew(event.mConversationThread);
          if (event.mFiredContactsChanged) {
            ZS_LOG_TRACE(log("firing delayed conversation thread"))
            mConversationThreadDelegate->onConversationThreadContactsChanged(event.mConversationThread);
          }

          for (ContactStateChangedList::iterator iter = event.mFiredContactStateChanges.begin(); iter != event.mFiredContactStateChanges.end(); ++iter)
          {
            ContactStateChangedEvent &changeEvent = (*iter);

            IContactPtr contact = changeEvent.first;
            ContactStates state = changeEvent.second;

            ZS_LOG_TRACE(log("firing delayed contact change") + ZS_PARAM("contact", contact->getID()) + ZS_PARAM("state", IConversationThread::toString(state)))
            mConversationThreadDelegate->onConversationThreadContactStateChanged(event.mConversationThread, contact, state);
          }
        } catch(IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        mFilteredEvents.erase(found);
      }
    }
  }
}
