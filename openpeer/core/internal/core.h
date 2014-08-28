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

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Backgrounding.h>
#include <openpeer/core/internal/core_Cache.h>
#include <openpeer/core/internal/core_Call.h>
#include <openpeer/core/internal/core_CallTransport.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_ConversationThreadHost.h>
#include <openpeer/core/internal/core_ConversationThreadHost_PeerContact.h>
#include <openpeer/core/internal/core_ConversationThreadHost_PeerLocation.h>
#include <openpeer/core/internal/core_ConversationThreadSlave.h>
#include <openpeer/core/internal/core_ConversationThreadDocumentFetcher.h>
//#include <openpeer/core/internal/stack_Decryptor.h>
#include <openpeer/core/internal/core_Encryptor.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/core/internal/core_Identity.h>
#include <openpeer/core/internal/core_IdentityLookup.h>
#include <openpeer/core/internal/core_Logger.h>
#include <openpeer/core/internal/core_MediaEngine.h>
#include <openpeer/core/internal/core_PushMailboxManager.h>
#include <openpeer/core/internal/core_PushMessaging.h>
#include <openpeer/core/internal/core_PushPresence.h>
#include <openpeer/core/internal/core_Settings.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_thread.h>
