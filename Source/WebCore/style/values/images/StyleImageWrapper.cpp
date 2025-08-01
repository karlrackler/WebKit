/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleImageWrapper.h"

#include "CSSValue.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

Ref<CSSValue> CSSValueCreation<ImageWrapper>::operator()(CSSValuePool&, const RenderStyle& style, const ImageWrapper& value)
{
    Ref image = value.value;
    return image->computedStyleValue(style);
}

// MARK: - Serialization

void Serialize<ImageWrapper>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const ImageWrapper& value)
{
    Ref image = value.value;
    builder.append(image->computedStyleValue(style)->cssText(context));
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const ImageWrapper& value)
{
    Ref image = value.value;

    ts << "image"_s;
    if (!image->url().resolved.isEmpty())
        ts << '(' << image->url().resolved << ')';
    return ts;
}

} // namespace Style
} // namespace WebCore
