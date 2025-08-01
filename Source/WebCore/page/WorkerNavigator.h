/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "JSDOMPromiseDeferredForward.h"
#include "NavigatorBase.h"
#include "Supplementable.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPU;
class NavigatorUAData;

class WorkerNavigator final : public NavigatorBase, public Supplementable<WorkerNavigator> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WorkerNavigator);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WorkerNavigator);
public:
    static Ref<WorkerNavigator> create(ScriptExecutionContext& context, const String& userAgent, bool isOnline) { return adoptRef(*new WorkerNavigator(context, userAgent, isOnline)); }

    virtual ~WorkerNavigator();

    const String& userAgent() const final;
    bool onLine() const final;
    void setIsOnline(bool isOnline) { m_isOnline = isOnline; }

    void setAppBadge(std::optional<unsigned long long>, Ref<DeferredPromise>&&);
    void clearAppBadge(Ref<DeferredPromise>&&);
    NavigatorUAData& userAgentData() const;

    GPU* gpu();

private:
    explicit WorkerNavigator(ScriptExecutionContext&, const String&, bool isOnline);

    void initializeNavigatorUAData() const;

    mutable RefPtr<NavigatorUAData> m_navigatorUAData;
    String m_userAgent;
    bool m_isOnline;
#if HAVE(WEBGPU_IMPLEMENTATION)
    RefPtr<GPU> m_gpuForWebGPU;
#endif
};

} // namespace WebCore
