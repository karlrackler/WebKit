/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InjectedBundleDOMWindowExtension.h"

#include "InjectedBundleScriptWorld.h"
#include "WebFrame.h"
#include "WebLocalFrameLoaderClient.h"
#include <WebCore/DOMWindowExtension.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrame.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

using ExtensionMap = HashMap<WeakRef<WebCore::DOMWindowExtension>, WeakRef<InjectedBundleDOMWindowExtension>>;
static ExtensionMap& allExtensions()
{
    static NeverDestroyed<ExtensionMap> map;
    return map;
}

Ref<InjectedBundleDOMWindowExtension> InjectedBundleDOMWindowExtension::create(WebFrame* frame, InjectedBundleScriptWorld* world)
{
    return adoptRef(*new InjectedBundleDOMWindowExtension(frame, world));
}

InjectedBundleDOMWindowExtension* InjectedBundleDOMWindowExtension::get(DOMWindowExtension* extension)
{
    ASSERT(allExtensions().contains(extension));
    return allExtensions().get(extension);
}

InjectedBundleDOMWindowExtension::InjectedBundleDOMWindowExtension(WebFrame* frame, InjectedBundleScriptWorld* world)
    : m_coreExtension(DOMWindowExtension::create(frame->coreLocalFrame() ? frame->coreLocalFrame()->window() : nullptr, world->coreWorld()))
{
    allExtensions().add(m_coreExtension.get(), *this);
}

InjectedBundleDOMWindowExtension::~InjectedBundleDOMWindowExtension()
{
    ASSERT(allExtensions().contains(m_coreExtension));
    allExtensions().remove(m_coreExtension);
}

RefPtr<WebFrame> InjectedBundleDOMWindowExtension::frame() const
{
    RefPtr frame = m_coreExtension->frame();
    return frame ? WebFrame::fromCoreFrame(*frame) : nullptr;
}

InjectedBundleScriptWorld* InjectedBundleDOMWindowExtension::world() const
{
    if (!m_world)
        m_world = InjectedBundleScriptWorld::getOrCreate(m_coreExtension->world());
        
    return m_world.get();
}

} // namespace WebKit
