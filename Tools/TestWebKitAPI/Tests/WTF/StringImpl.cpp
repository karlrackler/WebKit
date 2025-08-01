/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/ExternalStringImpl.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, StringImplCreationFromLiteral)
{
    // Constructor taking an ASCIILiteral.
    auto stringWithTemplate = StringImpl::create("Template Literal"_s);
    ASSERT_EQ(strlen("Template Literal"), stringWithTemplate->length());
    ASSERT_TRUE(equal(stringWithTemplate.get(), "Template Literal"_s));
    ASSERT_TRUE(stringWithTemplate->is8Bit());

    // Constructor taking the size explicitly.
    const char* programmaticStringData = "Explicit Size Literal";
    auto programmaticString = StringImpl::createWithoutCopying(unsafeSpan(programmaticStringData));
    ASSERT_EQ(strlen(programmaticStringData), programmaticString->length());
    ASSERT_TRUE(equal(programmaticString.get(), StringView::fromLatin1(programmaticStringData)));
    ASSERT_EQ(programmaticStringData, reinterpret_cast<const char*>(programmaticString->span8().data()));
    ASSERT_TRUE(programmaticString->is8Bit());

    // AtomStringImpl from createWithoutCopying should use the same underlying string.
    auto atomStringWithTemplate = AtomStringImpl::add(stringWithTemplate.ptr());
    ASSERT_TRUE(atomStringWithTemplate->is8Bit());
    ASSERT_EQ(atomStringWithTemplate->span8().data(), stringWithTemplate->span8().data());
    auto atomicProgrammaticString = AtomStringImpl::add(programmaticString.ptr());
    ASSERT_TRUE(atomicProgrammaticString->is8Bit());
    ASSERT_EQ(atomicProgrammaticString->span8().data(), programmaticString->span8().data());
}

TEST(WTF, StringImplReplaceWithLiteral)
{
    auto testStringImpl = StringImpl::create("1224"_s);
    ASSERT_TRUE(testStringImpl->is8Bit());

    // Cases for 8Bit source.
    testStringImpl = testStringImpl->replace('2', ""_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "14"_s));

    testStringImpl = StringImpl::create("1224"_s);
    ASSERT_TRUE(testStringImpl->is8Bit());

    testStringImpl = testStringImpl->replace('3', "NotFound"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "1224"_s));

    testStringImpl = testStringImpl->replace('2', "3"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "1334"_s));

    testStringImpl = StringImpl::create("1224"_s);
    ASSERT_TRUE(testStringImpl->is8Bit());
    testStringImpl = testStringImpl->replace('2', "555"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "15555554"_s));

    // Cases for 16Bit source.
    String testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());

    testStringImpl = testString.impl()->replace('2', "NotFound"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), String::fromUTF8("résumé").impl()));

    testStringImpl = testString.impl()->replace(char16_t(0x00E9 /*U+00E9 is 'é'*/), "e"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "resume"_s));

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());
    testStringImpl = testString.impl()->replace(char16_t(0x00E9 /*U+00E9 is 'é'*/), ""_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "rsum"_s));

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());
    testStringImpl = testString.impl()->replace(char16_t(0x00E9 /*U+00E9 is 'é'*/), "555"_span);
    ASSERT_TRUE(equal(testStringImpl.get(), "r555sum555"_s));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseBasic)
{
    auto a = StringImpl::create("aBcDeFG"_s);
    auto b = StringImpl::create("ABCDEFG"_s);
    auto c = StringImpl::create("abcdefg"_s);
    constexpr auto d = "aBcDeFG"_s;
    auto empty = StringImpl::create(""_span);
    auto shorter = StringImpl::create("abcdef"_s);
    auto different = StringImpl::create("abcrefg"_s);

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), a.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.ptr(), b.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.ptr(), c.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), d));
    ASSERT_TRUE(equalIgnoringASCIICase(b.ptr(), d));
    ASSERT_TRUE(equalIgnoringASCIICase(c.ptr(), d));

    // Transitivity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), b.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.ptr(), c.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), c.ptr()));

    // Negative cases.
    ASSERT_FALSE(equalIgnoringASCIICase(a.ptr(), empty.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.ptr(), empty.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.ptr(), empty.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(a.ptr(), shorter.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.ptr(), shorter.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.ptr(), shorter.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(a.ptr(), different.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.ptr(), different.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.ptr(), different.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(empty.ptr(), d));
    ASSERT_FALSE(equalIgnoringASCIICase(shorter.ptr(), d));
    ASSERT_FALSE(equalIgnoringASCIICase(different.ptr(), d));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithNull)
{
    auto reference = StringImpl::create("aBcDeFG"_s);
    StringImpl* nullStringImpl = nullptr;
    ASSERT_FALSE(equalIgnoringASCIICase(nullStringImpl, reference.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(reference.ptr(), nullStringImpl));
    ASSERT_TRUE(equalIgnoringASCIICase(nullStringImpl, nullStringImpl));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithEmpty)
{
    auto a = StringImpl::create(""_span);
    auto b = StringImpl::create(""_span);
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), b.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.ptr(), a.ptr()));
}

static Ref<StringImpl> stringFromUTF8(const char* characters)
{
    return String::fromUTF8(characters).releaseImpl().releaseNonNull();
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithLatin1Characters)
{
    auto a = stringFromUTF8("aBcéeFG");
    auto b = stringFromUTF8("ABCÉEFG");
    auto c = stringFromUTF8("ABCéEFG");
    auto d = stringFromUTF8("abcéefg");

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), a.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.ptr(), b.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.ptr(), c.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(d.ptr(), d.ptr()));

    // All combination.
    ASSERT_FALSE(equalIgnoringASCIICase(a.ptr(), b.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), c.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.ptr(), d.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.ptr(), c.ptr()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.ptr(), d.ptr()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.ptr(), d.ptr()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseBasic)
{
    auto referenceA = stringFromUTF8("aBcéeFG");
    auto referenceB = stringFromUTF8("ABCÉEFG");

    // Search the exact string.
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(referenceA.ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(referenceB.ptr()));

    // A and B are distinct by the non-ascii character é/É.
    EXPECT_EQ(static_cast<size_t>(notFound), referenceA->findIgnoringASCIICase(referenceB.ptr()));
    EXPECT_EQ(static_cast<size_t>(notFound), referenceB->findIgnoringASCIICase(referenceA.ptr()));

    // Find the prefix.
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(StringImpl::create("a"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(stringFromUTF8("abcé").ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(StringImpl::create("A"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(stringFromUTF8("ABCé").ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(StringImpl::create("a"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(stringFromUTF8("abcÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(StringImpl::create("A"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr()));

    // Not a prefix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::create("x"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("accé").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("abcÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::create("X"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ABDé").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::create("y"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("accÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("abcé").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::create("Y"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ABdÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ABCé").ptr()));

    // Find the infix.
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("cée").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("ée").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("cé").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("c").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("é").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("Cée").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éE").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("Cé").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("C").ptr()));

    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("cÉe").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("Ée").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("cÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("c").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("É").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("CÉe").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("ÉE").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("CÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("C").ptr()));

    // Not an infix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("céd").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Ée").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("bé").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("x").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("É").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("CÉe").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("éd").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("CÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Y").ptr()));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("cée").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("Éc").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("cé").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("W").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("é").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("bÉe").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éE").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("BÉ").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("z").ptr()));

    // Find the suffix.
    EXPECT_EQ(static_cast<size_t>(6), referenceA->findIgnoringASCIICase(StringImpl::create("g"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(4), referenceA->findIgnoringASCIICase(stringFromUTF8("efg").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éefg").ptr()));
    EXPECT_EQ(static_cast<size_t>(6), referenceA->findIgnoringASCIICase(StringImpl::create("G"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(4), referenceA->findIgnoringASCIICase(stringFromUTF8("EFG").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éEFG").ptr()));

    EXPECT_EQ(static_cast<size_t>(6), referenceB->findIgnoringASCIICase(StringImpl::create("g"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(4), referenceB->findIgnoringASCIICase(stringFromUTF8("efg").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("Éefg").ptr()));
    EXPECT_EQ(static_cast<size_t>(6), referenceB->findIgnoringASCIICase(StringImpl::create("G"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(4), referenceB->findIgnoringASCIICase(stringFromUTF8("EFG").ptr()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("ÉEFG").ptr()));

    // Not a suffix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::create("X"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("edg").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Éefg").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::create("w"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("dFG").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ÉEFG").ptr()));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::create("Z"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ffg").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éefg").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::create("r"_s).ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("EgG").ptr()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éEFG").ptr()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithValidOffset)
{
    auto reference = stringFromUTF8("ABCÉEFGaBcéeFG");
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(stringFromUTF8("ABC").ptr(), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABC").ptr(), 1));
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr(), 0));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr(), 1));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABCé").ptr(), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABCé").ptr(), 1));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithInvalidOffset)
{
    auto reference = stringFromUTF8("ABCÉEFGaBcéeFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABC").ptr(), 15));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABC").ptr(), 16));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr(), 17));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr(), 42));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").ptr(), std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseOnNull)
{
    auto reference = stringFromUTF8("ABCÉEFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, 0));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, 3));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, 7));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, 8));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, 42));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(StringView { }, std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseOnEmpty)
{
    auto reference = stringFromUTF8("ABCÉEFG");
    auto empty = StringImpl::create(""_span);
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(empty.ptr()));
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(empty.ptr(), 0));
    EXPECT_EQ(static_cast<size_t>(3), reference->findIgnoringASCIICase(empty.ptr(), 3));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.ptr(), 7));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.ptr(), 8));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.ptr(), 42));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.ptr(), std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithPatternLongerThanReference)
{
    auto reference = stringFromUTF8("ABCÉEFG");
    auto pattern = stringFromUTF8("XABCÉEFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(pattern.ptr()));
    EXPECT_EQ(static_cast<size_t>(1), pattern->findIgnoringASCIICase(reference.ptr()));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseBasic)
{
    auto reference = stringFromUTF8("aBcéX");
    auto referenceEquivalent = stringFromUTF8("AbCéx");

    // Identity.
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*reference.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(referenceEquivalent.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*referenceEquivalent.ptr()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(*reference.ptr()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(referenceEquivalent.ptr()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(*referenceEquivalent.ptr()));

    // Proper prefixes.
    auto aLower = StringImpl::create("a"_s);
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(aLower.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*aLower.ptr()));
    auto aUpper = StringImpl::create("A"_s);
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(aUpper.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*aUpper.ptr()));

    auto abcLower = StringImpl::create("abc"_s);
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcLower.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcLower.ptr()));
    auto abcUpper = StringImpl::create("ABC"_s);
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcUpper.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcUpper.ptr()));

    auto abcAccentLower = stringFromUTF8("abcé");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcAccentLower.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcAccentLower.ptr()));
    auto abcAccentUpper = stringFromUTF8("ABCé");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcAccentUpper.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcAccentUpper.ptr()));

    // Negative cases.
    auto differentFirstChar = stringFromUTF8("bBcéX");
    auto differentFirstCharProperPrefix = stringFromUTF8("CBcé");
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(differentFirstChar.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*differentFirstChar.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(differentFirstCharProperPrefix.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*differentFirstCharProperPrefix.ptr()));

    auto uppercaseAccent = stringFromUTF8("aBcÉX");
    auto uppercaseAccentProperPrefix = stringFromUTF8("aBcÉX");
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(uppercaseAccent.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*uppercaseAccent.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(uppercaseAccentProperPrefix.ptr()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*uppercaseAccentProperPrefix.ptr()));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseWithNull)
{
    auto reference = StringImpl::create("aBcDeFG"_s);
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(StringView { }));

    auto empty = StringImpl::create(""_span);
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(StringView { }));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseWithEmpty)
{
    auto reference = StringImpl::create("aBcDeFG"_s);
    auto empty = StringImpl::create(""_span);
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(empty.ptr()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*empty.ptr()));
    ASSERT_TRUE(empty->startsWithIgnoringASCIICase(empty.ptr()));
    ASSERT_TRUE(empty->startsWithIgnoringASCIICase(*empty.ptr()));
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(*reference.ptr()));
}

TEST(WTF, StartsWithLettersIgnoringASCIICase)
{
    String string("Test tEST"_s);
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test t"_s));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test te"_s));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test test"_s));
    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(string, "test tex"_s));

    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, ""_s));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(emptyString(), ""_s));

    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(String(), "t"_s));
    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(String(), ""_s));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseBasic)
{
    auto reference = stringFromUTF8("XÉCbA");
    auto referenceEquivalent = stringFromUTF8("xÉcBa");

    // Identity.
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*reference.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(referenceEquivalent.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*referenceEquivalent.ptr()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(*reference.ptr()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(referenceEquivalent.ptr()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(*referenceEquivalent.ptr()));

    // Proper suffixes.
    auto aLower = StringImpl::create("a"_s);
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(aLower.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*aLower.ptr()));
    auto aUpper = StringImpl::create("a"_s);
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(aUpper.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*aUpper.ptr()));

    auto abcLower = StringImpl::create("cba"_s);
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcLower.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcLower.ptr()));
    auto abcUpper = StringImpl::create("CBA"_s);
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcUpper.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcUpper.ptr()));

    auto abcAccentLower = stringFromUTF8("Écba");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcAccentLower.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcAccentLower.ptr()));
    auto abcAccentUpper = stringFromUTF8("ÉCBA");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcAccentUpper.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcAccentUpper.ptr()));

    // Negative cases.
    auto differentLastChar = stringFromUTF8("XÉCbB");
    auto differentLastCharProperSuffix = stringFromUTF8("ÉCbb");
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(differentLastChar.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*differentLastChar.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(differentLastCharProperSuffix.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*differentLastCharProperSuffix.ptr()));

    auto lowercaseAccent = stringFromUTF8("aBcéX");
    auto loweraseAccentProperSuffix = stringFromUTF8("aBcéX");
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(lowercaseAccent.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*lowercaseAccent.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(loweraseAccentProperSuffix.ptr()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*loweraseAccentProperSuffix.ptr()));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseWithNull)
{
    auto reference = StringImpl::create("aBcDeFG"_s);
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(StringView { }));

    auto empty = StringImpl::create(""_span);
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(StringView { }));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseWithEmpty)
{
    auto reference = StringImpl::create("aBcDeFG"_s);
    auto empty = StringImpl::create(""_span);
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(empty.ptr()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*empty.ptr()));
    ASSERT_TRUE(empty->endsWithIgnoringASCIICase(empty.ptr()));
    ASSERT_TRUE(empty->endsWithIgnoringASCIICase(*empty.ptr()));
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(reference.ptr()));
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(*reference.ptr()));
}

TEST(WTF, StringImplCreateNullSymbol)
{
    auto reference = SymbolImpl::createNullSymbol();
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isPrivate());
    ASSERT_TRUE(reference->isNullSymbol());
    ASSERT_FALSE(reference->isAtom());
    ASSERT_EQ(0u, reference->length());
    ASSERT_TRUE(equal(reference.ptr(), ""));
}

TEST(WTF, StringImplCreateSymbol)
{
    auto original = stringFromUTF8("original");
    auto reference = SymbolImpl::create(original);
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isPrivate());
    ASSERT_FALSE(reference->isNullSymbol());
    ASSERT_FALSE(reference->isAtom());
    ASSERT_FALSE(original->isSymbol());
    ASSERT_FALSE(original->isAtom());
    ASSERT_EQ(original->length(), reference->length());
    ASSERT_TRUE(equal(reference.ptr(), "original"));

    auto empty = stringFromUTF8("");
    auto emptyReference = SymbolImpl::create(empty);
    ASSERT_TRUE(emptyReference->isSymbol());
    ASSERT_FALSE(emptyReference->isPrivate());
    ASSERT_FALSE(emptyReference->isNullSymbol());
    ASSERT_FALSE(emptyReference->isAtom());
    ASSERT_FALSE(empty->isSymbol());
    ASSERT_TRUE(empty->isAtom());
    ASSERT_EQ(empty->length(), emptyReference->length());
    ASSERT_TRUE(equal(emptyReference.ptr(), ""));
}

TEST(WTF, StringImplCreatePrivateSymbol)
{
    auto original = stringFromUTF8("original");
    auto reference = PrivateSymbolImpl::create(original);
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_TRUE(reference->isPrivate());
    ASSERT_FALSE(reference->isNullSymbol());
    ASSERT_FALSE(reference->isAtom());
    ASSERT_FALSE(original->isSymbol());
    ASSERT_FALSE(original->isAtom());
    ASSERT_EQ(original->length(), reference->length());
    ASSERT_TRUE(equal(reference.ptr(), "original"));

    auto empty = stringFromUTF8("");
    auto emptyReference = PrivateSymbolImpl::create(empty);
    ASSERT_TRUE(emptyReference->isSymbol());
    ASSERT_TRUE(emptyReference->isPrivate());
    ASSERT_FALSE(emptyReference->isNullSymbol());
    ASSERT_FALSE(emptyReference->isAtom());
    ASSERT_FALSE(empty->isSymbol());
    ASSERT_TRUE(empty->isAtom());
    ASSERT_EQ(empty->length(), emptyReference->length());
    ASSERT_TRUE(equal(emptyReference.ptr(), ""));
}

TEST(WTF, StringImplSymbolToAtomString)
{
    auto original = stringFromUTF8("original");
    auto reference = SymbolImpl::create(original);
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isPrivate());
    ASSERT_FALSE(reference->isAtom());

    auto result = AtomStringImpl::lookUp(reference.ptr());
    ASSERT_FALSE(result);

    auto atomic = AtomStringImpl::add(reference.ptr());
    ASSERT_TRUE(atomic->isAtom());
    ASSERT_FALSE(atomic->isSymbol());
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtom());

    auto result2 = AtomStringImpl::lookUp(reference.ptr());
    ASSERT_TRUE(result2);
}

TEST(WTF, StringImplNullSymbolToAtomString)
{
    auto reference = SymbolImpl::createNullSymbol();
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isPrivate());
    ASSERT_FALSE(reference->isAtom());

    // Because the substring of the reference is the empty string which is already interned.
    auto result = AtomStringImpl::lookUp(reference.ptr());
    ASSERT_TRUE(result);

    auto atomic = AtomStringImpl::add(reference.ptr());
    ASSERT_TRUE(atomic->isAtom());
    ASSERT_FALSE(atomic->isSymbol());
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtom());
    ASSERT_EQ(atomic.get(), StringImpl::empty());

    auto result2 = AtomStringImpl::lookUp(reference.ptr());
    ASSERT_TRUE(result2);
}

static StringImpl::StaticStringImpl staticString {"Cocoa"};

TEST(WTF, StringImplStaticToAtomString)
{
    StringImpl& original = staticString;
    ASSERT_FALSE(original.isSymbol());
    ASSERT_FALSE(original.isAtom());
    ASSERT_TRUE(original.isStatic());

    auto result = AtomStringImpl::lookUp(&original);
    ASSERT_FALSE(result);

    auto atomic = AtomStringImpl::add(&original);
    ASSERT_TRUE(atomic->isAtom());
    ASSERT_FALSE(atomic->isSymbol());
    ASSERT_FALSE(atomic->isStatic());
    ASSERT_FALSE(original.isSymbol());
    ASSERT_FALSE(original.isAtom());
    ASSERT_TRUE(original.isStatic());

    ASSERT_TRUE(atomic->is8Bit());
    ASSERT_EQ(atomic->span8().data(), original.span8().data());

    auto result2 = AtomStringImpl::lookUp(&original);
    ASSERT_TRUE(result2);
    ASSERT_EQ(atomic, result2);
}

TEST(WTF, StringImplConstexprHasher)
{
    ASSERT_EQ(stringFromUTF8("")->hash(), StringHasher::computeLiteralHashAndMaskTop8Bits(""));
    ASSERT_EQ(stringFromUTF8("A")->hash(), StringHasher::computeLiteralHashAndMaskTop8Bits("A"));
    ASSERT_EQ(stringFromUTF8("AA")->hash(), StringHasher::computeLiteralHashAndMaskTop8Bits("AA"));
    ASSERT_EQ(stringFromUTF8("Cocoa")->hash(), StringHasher::computeLiteralHashAndMaskTop8Bits("Cocoa"));
    ASSERT_EQ(stringFromUTF8("Cappuccino")->hash(), StringHasher::computeLiteralHashAndMaskTop8Bits("Cappuccino"));
}

TEST(WTF, StringImplEmpty)
{
    ASSERT_FALSE(StringImpl::empty()->length());
}

static const String& neverDestroyedString()
{
    static NeverDestroyed<String> str(MAKE_STATIC_STRING_IMPL("NeverDestroyedString"));
    return str;
};

static const String& getNeverDestroyedStringAtStackDepth(int i)
{
    if (--i)
        return getNeverDestroyedStringAtStackDepth(i);
    return neverDestroyedString();
};

enum class StaticStringImplTestSet {
    StaticallyAllocatedImpl,
    DynamicallyAllocatedImpl
};

static void doStaticStringImplTests(StaticStringImplTestSet testSet, String& hello, String& world, String& longer, String& hello2)
{
    ASSERT_EQ(strlen("hello"), hello.length());
    ASSERT_EQ(strlen("world"), world.length());
    ASSERT_EQ(strlen("longer"), longer.length());
    ASSERT_EQ(strlen("hello"), hello2.length());

    ASSERT_TRUE(equal(hello, "hello"_s));
    ASSERT_TRUE(equal(world, "world"_s));
    ASSERT_TRUE(equal(longer, "longer"_s));
    ASSERT_TRUE(equal(hello2, "hello"_s));

    // Each StaticStringImpl* returned by MAKE_STATIC_STRING_IMPL should be unique.
    ASSERT_NE(hello.impl(), hello2.impl());

    if (testSet == StaticStringImplTestSet::StaticallyAllocatedImpl) {
        // Test that MAKE_STATIC_STRING_IMPL isn't allocating a StaticStringImpl on the stack.
        const String& str1 = getNeverDestroyedStringAtStackDepth(10);
        ASSERT_EQ(strlen("NeverDestroyedString"), str1.length());
        ASSERT_TRUE(equal(str1, "NeverDestroyedString"_s));

        const String& str2 = getNeverDestroyedStringAtStackDepth(20);
        ASSERT_EQ(strlen("NeverDestroyedString"), str2.length());
        ASSERT_TRUE(equal(str2, "NeverDestroyedString"_s));

        ASSERT_TRUE(equal(str1, str2));
        ASSERT_EQ(&str1, &str2);
        ASSERT_EQ(str1.impl(), str2.impl());
    }

    // Test that the StaticStringImpl's hash has already been set.
    // We're relying on an ASSERT in setHash() to detect that the hash hasn't
    // already been set. If the hash has already been set, the hash() method
    // will not call setHash().
    ASSERT_EQ(hello.hash(), 0xd17551u);
}

TEST(WTF, StaticStringImpl)
{
    // Construct using MAKE_STATIC_STRING_IMPL.
    String hello(MAKE_STATIC_STRING_IMPL("hello"));
    String world(MAKE_STATIC_STRING_IMPL("world"));
    String longer(MAKE_STATIC_STRING_IMPL("longer"));
    String hello2(MAKE_STATIC_STRING_IMPL("hello"));

    doStaticStringImplTests(StaticStringImplTestSet::StaticallyAllocatedImpl, hello, world, longer, hello2);
}

TEST(WTF, DynamicStaticStringImpl)
{
    // Construct using MAKE_STATIC_STRING_IMPL.
    String hello = StringImpl::createStaticStringImpl("hello"_span);
    String world = StringImpl::createStaticStringImpl("world"_span);
    String longer = StringImpl::createStaticStringImpl("longer"_span);
    String hello2 = StringImpl::createStaticStringImpl("hello"_span);

    doStaticStringImplTests(StaticStringImplTestSet::DynamicallyAllocatedImpl, hello, world, longer, hello2);
}

static SymbolImpl::StaticSymbolImpl staticSymbol {"Cocoa"};
static SymbolImpl::StaticSymbolImpl staticPrivateSymbol {"Cocoa", SymbolImpl::s_flagIsPrivate };

TEST(WTF, StaticSymbolImpl)
{
    auto& symbol = static_cast<SymbolImpl&>(staticSymbol);
    ASSERT_TRUE(symbol.isSymbol());
    ASSERT_FALSE(symbol.isPrivate());
}

TEST(WTF, StaticPrivateSymbolImpl)
{
    auto& symbol = static_cast<SymbolImpl&>(staticPrivateSymbol);
    ASSERT_TRUE(symbol.isSymbol());
    ASSERT_TRUE(symbol.isPrivate());
}

TEST(WTF, ExternalStringImplCreate8bit)
{
    constexpr LChar buffer[] = "hello";
    constexpr size_t bufferStringLength = sizeof(buffer) - 1;
    bool freeFunctionCalled = false;

    {
        auto external = ExternalStringImpl::create({ buffer, bufferStringLength }, [&freeFunctionCalled](ExternalStringImpl* externalStringImpl, void* buffer, unsigned bufferSize) mutable {
            freeFunctionCalled = true;
        });

        ASSERT_TRUE(external->isExternal());
        ASSERT_TRUE(external->is8Bit());
        ASSERT_FALSE(external->isSymbol());
        ASSERT_FALSE(external->isAtom());
        ASSERT_EQ(external->length(), bufferStringLength);
        ASSERT_EQ(external->span8().data(), buffer);
    }

    ASSERT_TRUE(freeFunctionCalled);
}

TEST(WTF, ExternalStringImplCreate16bit)
{
    constexpr char16_t buffer[] = { L'h', L'e', L'l', L'l', L'o', L'\0' };
    constexpr size_t bufferStringLength = (sizeof(buffer) - 1) / sizeof(char16_t);
    bool freeFunctionCalled = false;

    {
        auto external = ExternalStringImpl::create({ buffer, bufferStringLength }, [&freeFunctionCalled](ExternalStringImpl* externalStringImpl, void* buffer, unsigned bufferSize) mutable {
            freeFunctionCalled = true;
        });

        ASSERT_TRUE(external->isExternal());
        ASSERT_FALSE(external->is8Bit());
        ASSERT_FALSE(external->isSymbol());
        ASSERT_FALSE(external->isAtom());
        ASSERT_EQ(external->length(), bufferStringLength);
        ASSERT_EQ(external->span16().data(), buffer);
    }

    ASSERT_TRUE(freeFunctionCalled);
}

TEST(WTF, StringImplNotExternal)
{
    auto notExternal = stringFromUTF8("hello");
    ASSERT_FALSE(notExternal->isExternal());
}


TEST(WTF, ExternalStringAtom)
{
    constexpr LChar buffer[] = "hello";
    constexpr size_t bufferStringLength = sizeof(buffer) - 1;
    bool freeFunctionCalled = false;

    {
        auto external = ExternalStringImpl::create({ buffer, bufferStringLength }, [&freeFunctionCalled](ExternalStringImpl* externalStringImpl, void* buffer, unsigned bufferSize) mutable {
            freeFunctionCalled = true;
        });    

        ASSERT_TRUE(external->isExternal());
        ASSERT_FALSE(external->isAtom());
        ASSERT_FALSE(external->isSymbol());
        ASSERT_TRUE(external->is8Bit());
        ASSERT_EQ(external->length(), bufferStringLength);
        ASSERT_EQ(external->span8().data(), buffer);

        auto result = AtomStringImpl::lookUp(external.ptr());
        ASSERT_FALSE(result);

        auto atomic = AtomStringImpl::add(external.ptr());
        ASSERT_TRUE(atomic->isExternal());
        ASSERT_TRUE(atomic->isAtom());
        ASSERT_FALSE(atomic->isSymbol());
        ASSERT_TRUE(atomic->is8Bit());
        ASSERT_EQ(atomic->length(), external->length());
        ASSERT_EQ(atomic->span8().data(), external->span8().data());

        auto result2 = AtomStringImpl::lookUp(external.ptr());
        ASSERT_TRUE(result2);
        ASSERT_EQ(atomic, result2);
    }

    ASSERT_TRUE(freeFunctionCalled);
}

TEST(WTF, ExternalStringToSymbol)
{
    static constexpr ASCIILiteral buffer = "hello"_s;
    bool freeFunctionCalled = false;

    {
        auto external = ExternalStringImpl::create(buffer.span8(), [&freeFunctionCalled](ExternalStringImpl* externalStringImpl, void* buffer, unsigned bufferSize) mutable {
            freeFunctionCalled = true;
        });

        ASSERT_TRUE(external->isExternal());
        ASSERT_FALSE(external->isSymbol());
        ASSERT_FALSE(external->isAtom());

        auto symbol = SymbolImpl::create(external);
        ASSERT_FALSE(symbol->isExternal());
        ASSERT_TRUE(symbol->isSymbol());
        ASSERT_FALSE(symbol->isAtom());
        ASSERT_FALSE(symbol->isPrivate());
        ASSERT_FALSE(symbol->isNullSymbol());
        ASSERT_EQ(external->length(), symbol->length());
        ASSERT_TRUE(equal(symbol.ptr(), buffer.span8()));
    }

    ASSERT_TRUE(freeFunctionCalled);
}

} // namespace TestWebKitAPI
