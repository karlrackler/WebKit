/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GlyphPage.h"

#include "Font.h"
#include "FontCascade.h"
#include <pal/spi/cf/CoreTextSPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

static bool shouldFillWithVerticalGlyphs(std::span<const char16_t> buffer, const Font& font)
{
    if (!font.hasVerticalGlyphs())
        return false;
    for (auto character : buffer) {
        if (!FontCascade::isCJKIdeograph(character))
            return true;
    }
    return false;
}


bool GlyphPage::fill(std::span<const char16_t> buffer)
{
    ASSERT(buffer.size() == GlyphPage::size || buffer.size() == 2 * GlyphPage::size);

    Ref<const Font> font = this->font();
    Vector<CGGlyph, 512> glyphs(buffer.size());
    unsigned glyphStep = buffer.size() / GlyphPage::size;

    if (shouldFillWithVerticalGlyphs(buffer, font))
        CTFontGetVerticalGlyphsForCharacters(font->platformData().ctFont(), reinterpret_cast<const UniChar*>(buffer.data()), glyphs.mutableSpan().data(), buffer.size());
    else
        CTFontGetGlyphsForCharacters(font->platformData().ctFont(), reinterpret_cast<const UniChar*>(buffer.data()), glyphs.mutableSpan().data(), buffer.size());

    bool haveGlyphs = false;
    for (unsigned i = 0; i < GlyphPage::size; ++i) {
        auto theGlyph = glyphs[i * glyphStep];
        if (theGlyph && theGlyph != deletedGlyph) {
            setGlyphForIndex(i, theGlyph, font->colorGlyphType(theGlyph));
            haveGlyphs = true;
        }
    }
    return haveGlyphs;
}

} // namespace WebCore
