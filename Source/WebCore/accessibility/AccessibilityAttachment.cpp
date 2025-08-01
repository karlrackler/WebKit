/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityAttachment.h"

#include "HTMLAttachmentElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "RenderAttachment.h"

#if ENABLE(ATTACHMENT_ELEMENT)

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityAttachment::AccessibilityAttachment(AXID axID, RenderAttachment& renderer, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, renderer, cache)
{
}

Ref<AccessibilityAttachment> AccessibilityAttachment::create(AXID axID, RenderAttachment& renderer, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityAttachment(axID, renderer, cache));
}

bool AccessibilityAttachment::hasProgress(float* progress) const
{
    auto& progressString = getAttribute(progressAttr);
    bool validProgress;
    float result = std::max<float>(std::min<float>(progressString.toFloat(&validProgress), 1), 0);
    if (progress)
        *progress = result;
    return validProgress;
}

float AccessibilityAttachment::valueForRange() const
{
    float progress = 0;
    hasProgress(&progress);
    return progress;
}
    
HTMLAttachmentElement* AccessibilityAttachment::attachmentElement() const
{
    ASSERT(is<HTMLAttachmentElement>(node()));
    return dynamicDowncast<HTMLAttachmentElement>(node());
}

bool AccessibilityAttachment::computeIsIgnored() const
{
    return false;
}
    
void AccessibilityAttachment::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
    RefPtr attachmentElement = this->attachmentElement();
    if (!attachmentElement)
        return;
    
    auto title = attachmentElement->attachmentTitle();
    auto& subtitle = attachmentElement->attachmentSubtitle();
    auto& action = getAttribute(actionAttr);
    
    if (action.length())
        textOrder.append(AccessibilityText(WTFMove(action), AccessibilityTextSource::Action));

    if (title.length())
        textOrder.append(AccessibilityText(WTFMove(title), AccessibilityTextSource::Title));

    if (subtitle.length())
        textOrder.append(AccessibilityText(WTFMove(subtitle), AccessibilityTextSource::Subtitle));
}

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)

