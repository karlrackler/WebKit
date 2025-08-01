/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "Editor.h"

#include "AXObjectCache.h"
#include "AlternativeTextController.h"
#include "ApplyStyleCommand.h"
#include "AttachmentAssociatedElement.h"
#include "BoundaryPointInlines.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSSerializationContext.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CachedResourceLoader.h"
#include "CaretRectComputation.h"
#include "ChangeListTypeCommand.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClipboardEvent.h"
#include "CommonAtomStrings.h"
#include "CompositionEvent.h"
#include "CompositionHighlight.h"
#include "ContainerNodeInlines.h"
#include "CreateLinkCommand.h"
#include "CustomUndoStep.h"
#include "DataTransfer.h"
#include "DeleteSelectionCommand.h"
#include "DeprecatedGlobalSettings.h"
#include "DictationAlternative.h"
#include "DictationCommand.h"
#include "DocumentFragment.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "Editing.h"
#include "EditorClient.h"
#include "ElementAncestorIteratorInlines.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "File.h"
#include "FocusController.h"
#include "FontAttributes.h"
#include "FrameInlines.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBRElement.h"
#include "HTMLBodyElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLQuoteElement.h"
#include "HTMLSpanElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLUListElement.h"
#include "HitTestResult.h"
#include "ImageOverlay.h"
#include "IndentOutdentCommand.h"
#include "InputEvent.h"
#include "InsertListCommand.h"
#include "InsertTextCommand.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "ModifySelectionListLevel.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PagePasteboardContext.h"
#include "Pasteboard.h"
#include "Quirks.h"
#include "Range.h"
#include "RemoveFormatCommand.h"
#include "RenderAncestorIterator.h"
#include "RenderBlock.h"
#include "RenderBlockFlow.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderTextControl.h"
#include "RenderedDocumentMarker.h"
#include "RenderedPosition.h"
#include "ReplaceRangeWithTextCommand.h"
#include "ReplaceSelectionCommand.h"
#include "ScriptDisallowedScope.h"
#include "SerializedAttachmentData.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include "SimplifyMarkupCommand.h"
#include "SpellChecker.h"
#include "SpellingCorrectionCommand.h"
#include "StaticPasteboard.h"
#include "StylePropertiesInlines.h"
#include "StyleTreeResolver.h"
#include "SystemSoundManager.h"
#include "TelephoneNumberDetector.h"
#include "Text.h"
#include "TextCheckerClient.h"
#include "TextCheckingHelper.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TextPlaceholderElement.h"
#include "TypingCommand.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleUnits.h"
#include "WritingSuggestionData.h"
#include "markup.h"
#include <pal/FileSizeFormatter.h>
#include <pal/text/KillRing.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
#include "PromisedAttachmentInfo.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TemporarySelectionChange);
WTF_MAKE_TZONE_ALLOCATED_IMPL(IgnoreSelectionChangeForScope);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Editor);

static bool dispatchBeforeInputEvent(Element& element, const AtomString& inputType, IsInputMethodComposing isInputMethodComposing, const String& data = { },
    RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { }, Event::IsCancelable cancelable = Event::IsCancelable::Yes)
{
    auto event = InputEvent::create(eventNames().beforeinputEvent, inputType, cancelable, element.document().windowProxy(), data,
        WTFMove(dataTransfer), targetRanges, 0, isInputMethodComposing);
    element.dispatchEvent(event);
    return !event->defaultPrevented();
}

static void dispatchInputEvent(Element& element, const AtomString& inputType, IsInputMethodComposing isInputMethodComposing, const String& data = { },
    RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { })
{
    // FIXME: We should not be dispatching to the scoped queue here. Normally, input events are dispatched in CompositeEditCommand::apply after the end of the scope,
    // but TypingCommands are special in that existing TypingCommands that are applied again fire input events *from within* the scope by calling typingAddedToOpenCommand.
    // Instead, TypingCommands should always dispatch events synchronously after the end of the scoped queue in CompositeEditCommand::apply. To work around this for the
    // time being, just revert back to calling dispatchScopedEvent.
    element.dispatchScopedEvent(InputEvent::create(eventNames().inputEvent, inputType, Event::IsCancelable::No, element.document().windowProxy(), data,
        WTFMove(dataTransfer), targetRanges, 0, isInputMethodComposing));
}

static String inputEventDataForEditingStyleAndAction(const StyleProperties* style, EditAction action)
{
    if (!style)
        return { };

    switch (action) {
    case EditAction::SetColor:
        return style->getPropertyValue(CSSPropertyColor);
    case EditAction::SetInlineWritingDirection:
    case EditAction::SetBlockWritingDirection:
        return style->getPropertyValue(CSSPropertyDirection);
    default:
        return { };
    }
}

static String inputEventDataForEditingStyleAndAction(EditingStyle& style, EditAction action)
{
    return inputEventDataForEditingStyleAndAction(style.style(), action);
}

class ClearTextCommand : public DeleteSelectionCommand {
public:
    ClearTextCommand(Ref<Document>&&);
    static void CreateAndApply(Ref<Document>&&);

private:
    EditAction editingAction() const override;
};

ClearTextCommand::ClearTextCommand(Ref<Document>&& document)
    : DeleteSelectionCommand(WTFMove(document), false, true, false, false, true)
{
}

EditAction ClearTextCommand::editingAction() const
{
    return EditAction::Delete;
}

void ClearTextCommand::CreateAndApply(Ref<Document>&& document)
{
    if (document->selection().isNone())
        return;

    // Don't leave around stale composition state.
    document->editor().clear();
    
    const VisibleSelection oldSelection = document->selection().selection();
    document->selection().selectAll();
    auto clearCommand = adoptRef(*new ClearTextCommand(WTFMove(document)));
    clearCommand->setStartingSelection(oldSelection);
    clearCommand->apply();
}

using namespace HTMLNames;
using namespace WTF::Unicode;

TemporarySelectionChange::TemporarySelectionChange(Document& document, std::optional<VisibleSelection> temporarySelection, OptionSet<TemporarySelectionOption> options)
    : m_document(&document)
    , m_options(options)
    , m_wasIgnoringSelectionChanges(document.editor().ignoreSelectionChanges())
#if PLATFORM(IOS_FAMILY)
    , m_appearanceUpdatesWereEnabled(document.selection().isUpdateAppearanceEnabled())
#endif
{
#if PLATFORM(IOS_FAMILY)
    if (options & TemporarySelectionOption::EnableAppearanceUpdates)
        document.selection().setUpdateAppearanceEnabled(true);
#endif

    if (options & TemporarySelectionOption::IgnoreSelectionChanges)
        document.editor().setIgnoreSelectionChanges(true);

    if (temporarySelection) {
        m_selectionToRestore = document.selection().selection();
#if PLATFORM(IOS_FAMILY)
        if (document.selection().isUpdateAppearanceEnabled())
            document.selection().setNeedsSelectionUpdate();
#endif
        setSelection(temporarySelection.value(), IsTemporarySelection::Yes);
    }
}

void TemporarySelectionChange::invalidate()
{
    if (RefPtr document = std::exchange(m_document, nullptr))
        document->editor().setIgnoreSelectionChanges(false, Editor::RevealSelection::No);
}

TemporarySelectionChange::~TemporarySelectionChange()
{
    if (!m_document)
        return;

    if (m_selectionToRestore)
        setSelection(m_selectionToRestore.value(), IsTemporarySelection::No);

    if (m_options & TemporarySelectionOption::IgnoreSelectionChanges) {
        auto revealSelection = m_options & TemporarySelectionOption::RevealSelection ? Editor::RevealSelection::Yes : Editor::RevealSelection::No;
        m_document->editor().setIgnoreSelectionChanges(m_wasIgnoringSelectionChanges, revealSelection);
    }

#if PLATFORM(IOS_FAMILY)
    if (m_options & TemporarySelectionOption::EnableAppearanceUpdates)
        m_document->selection().setUpdateAppearanceEnabled(m_appearanceUpdatesWereEnabled);
#endif
}

void TemporarySelectionChange::setSelection(const VisibleSelection& selection, IsTemporarySelection isTemporarySelection)
{
    auto options = FrameSelection::defaultSetSelectionOptions();

    if (m_options & TemporarySelectionOption::UserTriggered)
        options.add(FrameSelection::SetSelectionOption::IsUserTriggered);

    if (m_options & TemporarySelectionOption::DoNotSetFocus)
        options.add(FrameSelection::SetSelectionOption::DoNotSetFocus);

    if (isTemporarySelection == IsTemporarySelection::Yes) {
        if (m_options & TemporarySelectionOption::RevealSelection)
            options.add(FrameSelection::SetSelectionOption::RevealSelection);
        if (m_options & TemporarySelectionOption::DelegateMainFrameScroll)
            options.add(FrameSelection::SetSelectionOption::DelegateMainFrameScroll);
        if (m_options & TemporarySelectionOption::SmoothScroll)
            options.add(FrameSelection::SetSelectionOption::SmoothScroll);
        if (m_options & TemporarySelectionOption::RevealSelectionBounds)
            options.add(FrameSelection::SetSelectionOption::RevealSelectionBounds);
        if (m_options & TemporarySelectionOption::ForceCenterScroll)
            options.add(FrameSelection::SetSelectionOption::ForceCenterScroll);
        if (m_options & TemporarySelectionOption::OnlyAllowForwardScrolling)
            options.add(FrameSelection::SetSelectionOption::OnlyAllowForwardScrolling);
    }

    m_document->selection().setSelection(selection, options);
}

IgnoreSelectionChangeForScope::IgnoreSelectionChangeForScope(LocalFrame& frame)
    : m_selectionChange(*frame.document(), std::nullopt, TemporarySelectionOption::IgnoreSelectionChanges)
{
}

IgnoreSelectionChangeForScope::~IgnoreSelectionChangeForScope() = default;

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
VisibleSelection Editor::selectionForCommand(Event* event)
{
    auto selection = document().selection().selection();
    if (!event)
        return selection;
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
    if (RefPtr target = dynamicDowncast<HTMLTextFormControlElement>(event->target()); target && target->isTextField()) {
        auto start = selection.start();
        if (start.isNull() || event->target() != enclosingTextFormControl(start)) {
            if (auto range = target->selection())
                return { *range, Affinity::Downstream, selection.directionality() };
        }
    }
    return selection;
}

// Function considers Mac editing behavior a fallback when Page or Settings is not available.
EditingBehavior Editor::behavior() const
{
    return document().editingBehavior();
}

EditorClient* Editor::client() const
{
    ASSERT(!m_client || !document().page() || m_client == &document().page()->editorClient());
    return m_client.get();
}

TextCheckerClient* Editor::textChecker() const
{
    if (EditorClient* owner = client())
        return owner->textChecker();
    return 0;
}

void Editor::handleKeyboardEvent(KeyboardEvent& event)
{
    if (auto* client = this->client())
        client->handleKeyboardEvent(event);
}

void Editor::handleInputMethodKeydown(KeyboardEvent& event)
{
    if (auto* client = this->client())
        client->handleInputMethodKeydown(event);
}

void Editor::didDispatchInputMethodKeydown(KeyboardEvent& event)
{
    if (auto* client = this->client())
        client->didDispatchInputMethodKeydown(event);
}

bool Editor::handleTextEvent(TextEvent& event)
{
    LOG(Editing, "Editor %p handleTextEvent (data %s)", this, event.data().utf8().data());

    // Default event handling for Drag and Drop will be handled by DragController
    // so we leave the event for it.
    if (event.isDrop())
        return false;

    if (event.isPaste() || event.isRemoveBackground()) {
        auto action = event.isRemoveBackground() ? EditAction::RemoveBackground : EditAction::Paste;
        if (event.pastingFragment()) {
#if PLATFORM(IOS_FAMILY)
            if (client()->performsTwoStepPaste(event.pastingFragment()))
                return true;
#endif
            replaceSelectionWithFragment(*event.pastingFragment(), SelectReplacement::No, event.shouldSmartReplace() ? SmartReplace::Yes : SmartReplace::No, event.shouldMatchStyle() ? MatchStyle::Yes : MatchStyle::No, action, event.mailBlockquoteHandling());
        } else
            replaceSelectionWithText(event.data(), SelectReplacement::No, event.shouldSmartReplace() ? SmartReplace::Yes : SmartReplace::No, action);
        return true;
    }

    String data = event.data();
    if (data == "\n"_s) {
        if (event.isLineBreak())
            return insertLineBreak();
        return insertParagraphSeparator();
    }

    return insertTextWithoutSendingTextEvent(data, false, &event);
}

bool Editor::canEdit() const
{
    return document().selection().selection().rootEditableElement();
}

bool Editor::canEditRichly() const
{
    return document().selection().selection().isContentRichlyEditable();
}

static AtomString eventNameForClipboardEvent(ClipboardEventKind kind)
{
    switch (kind) {
    case ClipboardEventKind::Copy:
    case ClipboardEventKind::CopyFont:
        return eventNames().copyEvent;
    case ClipboardEventKind::Cut:
        return eventNames().cutEvent;
    case ClipboardEventKind::Paste:
    case ClipboardEventKind::PasteAsPlainText:
    case ClipboardEventKind::PasteAsQuotation:
    case ClipboardEventKind::PasteFont:
        return eventNames().pasteEvent;
    case ClipboardEventKind::BeforeCopy:
        return eventNames().beforecopyEvent;
    case ClipboardEventKind::BeforeCut:
        return eventNames().beforecutEvent;
    case ClipboardEventKind::BeforePaste:
        return eventNames().beforepasteEvent;
    }
    ASSERT_NOT_REACHED();
    return { };
}

static Ref<DataTransfer> createDataTransferForClipboardEvent(Document& document, ClipboardEventKind kind)
{
    switch (kind) {
    case ClipboardEventKind::Copy:
    case ClipboardEventKind::CopyFont:
    case ClipboardEventKind::Cut:
        return DataTransfer::createForCopyAndPaste(document, DataTransfer::StoreMode::ReadWrite, makeUnique<StaticPasteboard>());
    case ClipboardEventKind::PasteAsPlainText:
        if (DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
            auto plainTextType = textPlainContentTypeAtom();
            auto plainText = Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document.pageID()))->readString(plainTextType);
            auto pasteboard = makeUnique<StaticPasteboard>();
            pasteboard->writeString(plainTextType, plainText);
            return DataTransfer::createForCopyAndPaste(document, DataTransfer::StoreMode::Readonly, WTFMove(pasteboard));
        }
        [[fallthrough]];
    case ClipboardEventKind::Paste:
    case ClipboardEventKind::PasteAsQuotation:
    case ClipboardEventKind::PasteFont:
        return DataTransfer::createForCopyAndPaste(document, DataTransfer::StoreMode::Readonly, Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document.pageID())));
    case ClipboardEventKind::BeforeCopy:
    case ClipboardEventKind::BeforeCut:
    case ClipboardEventKind::BeforePaste:
        return DataTransfer::createForCopyAndPaste(document, DataTransfer::StoreMode::Invalid, makeUnique<StaticPasteboard>());
    }
    ASSERT_NOT_REACHED();
    return DataTransfer::createForCopyAndPaste(document, DataTransfer::StoreMode::Invalid, makeUnique<StaticPasteboard>());
}

// Returns whether caller should continue with "the default processing", which is the same as
// the event handler NOT setting the return value to false
// https://w3c.github.io/clipboard-apis/#fire-a-clipboard-event
bool Editor::dispatchClipboardEvent(RefPtr<Element>&& target, ClipboardEventKind kind, Ref<DataTransfer>&& dataTransfer)
{
    // FIXME: Move the target selection code here.
    if (!target)
        return true;

    auto event = ClipboardEvent::create(eventNameForClipboardEvent(kind), dataTransfer.copyRef());

    target->dispatchEvent(event);
    bool noDefaultProcessing = event->defaultPrevented();
    if (noDefaultProcessing && (kind == ClipboardEventKind::Copy || kind == ClipboardEventKind::Cut) && dataTransfer->pasteboard().hasData())
        dataTransfer->commitToPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(target->document().pageID())));

    dataTransfer->makeInvalidForSecurity();

    return !noDefaultProcessing;
}

bool Editor::dispatchClipboardEvent(RefPtr<Element>&& target, ClipboardEventKind kind)
{
    if (!target)
        return true;

    return Editor::dispatchClipboardEvent(WTFMove(target), kind, createDataTransferForClipboardEvent(target->document(), kind));
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items.  They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool Editor::canDHTMLCut()
{
    if (document().selection().selection().isInPasswordField())
        return false;

    return !dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::BeforeCut);
}

bool Editor::canDHTMLCopy()
{
    if (document().selection().selection().isInPasswordField())
        return false;
    return !dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::BeforeCopy);
}

bool Editor::canDHTMLPaste()
{
    return !dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::BeforePaste);
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

static HTMLImageElement* imageElementFromImageDocument(Document& document)
{
    if (!document.isImageDocument())
        return nullptr;
    
    RefPtr body = document.bodyOrFrameset();
    return body ? dynamicDowncast<HTMLImageElement>(body->firstChild()) : nullptr;
}

bool Editor::canCopy() const
{
    if (imageElementFromImageDocument(document()))
        return true;
    const VisibleSelection& selection = document().selection().selection();
    return (selection.isRange() || !isEditablePosition(selection.start())) && (!selection.isInPasswordField() || selection.isInAutoFilledAndViewableField());
}

bool Editor::canDelete() const
{
    const VisibleSelection& selection = document().selection().selection();
    return selection.isRange() && selection.rootEditableElement();
}

bool Editor::canDeleteRange(const SimpleRange& range) const
{
    if (!range.start.container->hasEditableStyle() || !range.end.container->hasEditableStyle())
        return false;

    if (range.collapsed()) {
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        auto previous = VisiblePosition { makeDeprecatedLegacyPosition(range.start) }.previous();
        if (previous.isNull() || previous.deepEquivalent().deprecatedNode()->rootEditableElement() != range.start.container->rootEditableElement())
            return false;
    }

    return true;
}

bool Editor::shouldSmartDelete()
{
    if (behavior().shouldAlwaysSmartDelete())
        return true;
    return document().selection().granularity() == TextGranularity::WordGranularity;
}

bool Editor::smartInsertDeleteEnabled()
{   
    return client() && client()->smartInsertDeleteEnabled();
}
    
bool Editor::canSmartCopyOrDelete()
{
    return client() && client()->smartInsertDeleteEnabled() && shouldSmartDelete();
}

bool Editor::isSelectTrailingWhitespaceEnabled() const
{
    return client() && client()->isSelectTrailingWhitespaceEnabled();
}

bool Editor::deleteWithDirection(SelectionDirection direction, TextGranularity granularity, bool shouldAddToKillRing, bool isTypingAction)
{
    if (!canEdit())
        return false;

    Ref document = this->document();
    if (document->selection().isRange()) {
        if (isTypingAction) {
            TypingCommand::deleteKeyPressed(WTFMove(document), canSmartCopyOrDelete() ? TypingCommand::Option::SmartDelete : OptionSet<TypingCommand::Option> { }, granularity);
            revealSelectionAfterEditingOperation();
        } else {
            if (shouldAddToKillRing)
                addRangeToKillRing(*selectedRange(), KillRingInsertionMode::AppendText);
            deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
            // Implicitly calls revealSelectionAfterEditingOperation().
        }
    } else {
        OptionSet<TypingCommand::Option> options;
        if (canSmartCopyOrDelete())
            options.add(TypingCommand::Option::SmartDelete);
        if (shouldAddToKillRing)
            options.add(TypingCommand::Option::AddsToKillRing);
        switch (direction) {
        case SelectionDirection::Forward:
        case SelectionDirection::Right:
            TypingCommand::forwardDeleteKeyPressed(WTFMove(document), options, granularity);
            break;
        case SelectionDirection::Backward:
        case SelectionDirection::Left:
            TypingCommand::deleteKeyPressed(WTFMove(document), options, granularity);
            break;
        }
        revealSelectionAfterEditingOperation();
    }

    // FIXME: We should to move this down into deleteKeyPressed.
    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    if (shouldAddToKillRing)
        setStartNewKillRingSequence(false);

    return true;
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete, EditAction editingAction)
{
    Ref document = this->document();
    if (document->selection().isNone())
        return;

    DeleteSelectionCommand::create(WTFMove(document), smartDelete, true, false, false, true, editingAction)->apply();
}

void Editor::clearText()
{
    ClearTextCommand::CreateAndApply(protectedDocument());
}

void Editor::pasteAsPlainText(const String& pastingText, bool smartReplace)
{
    auto target = findEventTargetFromSelection();
    if (!target)
        return;
    auto sanitizedText = pastingText;
    Ref document = this->document();
    if (RefPtr page = document->page())
        sanitizedText = page->applyLinkDecorationFiltering(sanitizedText, LinkDecorationFilteringTrigger::Paste);
    target->dispatchEvent(TextEvent::createForPlainTextPaste(document->windowProxy(), WTFMove(sanitizedText), smartReplace));
}

void Editor::pasteAsFragment(Ref<DocumentFragment>&& pastingFragment, bool smartReplace, bool matchStyle, MailBlockquoteHandling respectsMailBlockquote, EditAction action)
{
    auto target = findEventTargetFromSelection();
    if (!target)
        return;

    ASSERT(action == EditAction::RemoveBackground || action == EditAction::Paste);
    auto type = action == EditAction::RemoveBackground ? TextEventInputRemoveBackground : TextEventInputPaste;
    target->dispatchEvent(TextEvent::createForFragmentPaste(document().windowProxy(), WTFMove(pastingFragment), type, smartReplace, matchStyle, respectsMailBlockquote));
}

void Editor::pasteAsPlainTextBypassingDHTML()
{
    pasteAsPlainTextWithPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID())));
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard& pasteboard)
{
    String text = readPlainTextFromPasteboard(pasteboard);
    if (client() && client()->shouldInsertText(text, selectedRange(), EditorInsertAction::Pasted))
        pasteAsPlainText(text, canSmartReplaceWithPasteboard(pasteboard));
}

String Editor::readPlainTextFromPasteboard(Pasteboard& pasteboard)
{
    PasteboardPlainText text;
    pasteboard.read(text);
    return plainTextFromPasteboard(text);
}

#if !PLATFORM(MAC)

String Editor::plainTextFromPasteboard(const PasteboardPlainText& text)
{
    return text.text;
}

#endif

bool Editor::canSmartReplaceWithPasteboard(Pasteboard& pasteboard)
{
    return client() && client()->smartInsertDeleteEnabled() && pasteboard.canSmartReplace();
}

bool Editor::shouldInsertFragment(DocumentFragment& fragment, const std::optional<SimpleRange>& replacingDOMRange, EditorInsertAction givenAction)
{
    if (!client())
        return false;
    
    if (RefPtr child = dynamicDowncast<CharacterData>(fragment.firstChild()); child && fragment.lastChild() == fragment.firstChild())
        return client()->shouldInsertText(child->data(), replacingDOMRange, givenAction);

    return client()->shouldInsertNode(fragment, replacingDOMRange, givenAction);
}

void Editor::replaceSelectionWithFragment(DocumentFragment& fragment, SelectReplacement selectReplacement, SmartReplace smartReplace, MatchStyle matchStyle, EditAction editingAction, MailBlockquoteHandling mailBlockquoteHandling)
{
    Ref document = this->document();
    VisibleSelection selection = document->selection().selection();
    if (selection.isNone() || !selection.isContentEditable())
        return;

    AccessibilityReplacedText replacedText;
    if (AXObjectCache::accessibilityEnabled() && (editingAction == EditAction::Paste || editingAction == EditAction::Insert || editingAction == EditAction::RemoveBackground))
        replacedText = AccessibilityReplacedText(selection);

    OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::PreventNesting, ReplaceSelectionCommand::SanitizeFragment };
    if (selectReplacement == SelectReplacement::Yes)
        options.add(ReplaceSelectionCommand::SelectReplacement);
    if (smartReplace == SmartReplace::Yes)
        options.add(ReplaceSelectionCommand::SmartReplace);
    if (matchStyle == MatchStyle::Yes)
        options.add(ReplaceSelectionCommand::MatchStyle);
    if (mailBlockquoteHandling == MailBlockquoteHandling::IgnoreBlockquote)
        options.add(ReplaceSelectionCommand::IgnoreMailBlockquote);

    auto command = ReplaceSelectionCommand::create(document.copyRef(), &fragment, options, editingAction);
    command->apply();

    m_imageElementsToLoadBeforeRevealingSelection.clear();
    if (auto insertionRange = command->insertedContentRange())
        m_imageElementsToLoadBeforeRevealingSelection = visibleImageElementsInRangeWithNonLoadedImages(*insertionRange);

    if (m_imageElementsToLoadBeforeRevealingSelection.isEmpty())
        revealSelectionAfterEditingOperation();

    selection = document->selection().selection();
    if (selection.isInPasswordField())
        return;

    if (AXObjectCache::accessibilityEnabled() && editingAction == EditAction::Paste) {
        String text = AccessibilityObject::stringForVisiblePositionRange(command->visibleSelectionForInsertedText());
        replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypePaste, text, document->selection().selection());
        command->composition()->setRangeDeletedByUnapply(replacedText.replacedRange());
    }

    if (AXObjectCache::accessibilityEnabled() && editingAction == EditAction::Insert) {
        String text = command->documentFragmentPlainText();
        replacedText.postTextStateChangeNotification(document->existingAXObjectCache(), AXTextEditTypeInsert, text, document->selection().selection());
        command->composition()->setRangeDeletedByUnapply(replacedText.replacedRange());
    }

    if (!isContinuousSpellCheckingEnabled())
        return;

    RefPtr nodeToCheck { selection.rootEditableElement() };
    if (!nodeToCheck)
        return;

    auto rangeToCheck = makeRangeSelectingNodeContents(*nodeToCheck);
    if (auto request = SpellCheckRequest::create(resolveTextCheckingTypeMask(*nodeToCheck, { TextCheckingType::Spelling, TextCheckingType::Grammar }), TextCheckingProcessType::TextCheckingProcessBatch, rangeToCheck, rangeToCheck, rangeToCheck))
        m_spellChecker->requestCheckingFor(request.releaseNonNull());
}

void Editor::replaceSelectionWithText(const String& text, SelectReplacement selectReplacement, SmartReplace smartReplace, EditAction editingAction)
{
    auto range = selectedRange();
    if (!range)
        return;

    replaceSelectionWithFragment(createFragmentFromText(*range, text), selectReplacement, smartReplace, MatchStyle::Yes, editingAction);
}

std::optional<SimpleRange> Editor::selectedRange()
{
    return document().selection().selection().toNormalizedRange();
}

bool Editor::shouldDeleteRange(const std::optional<SimpleRange>& range) const
{
    return range && !range->collapsed() && canDeleteRange(*range) && client() && client()->shouldDeleteRange(*range);
}

bool Editor::tryDHTMLCopy()
{
    if (document().selection().selection().isInPasswordField())
        return false;

    return !dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::Copy);
}

bool Editor::tryDHTMLCut()
{
    if (document().selection().selection().isInPasswordField())
        return false;
    
    return !dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::Cut);
}

bool Editor::shouldInsertText(const String& text, const std::optional<SimpleRange>& range, EditorInsertAction action) const
{
    // FIXME(273431): shouldSuppressTextInputFromEditing does not work with site isolation.
    RefPtr localFrame = document().localMainFrame();
    if (localFrame && localFrame->loader().shouldSuppressTextInputFromEditing() && action == EditorInsertAction::Typed)
        return false;

    return client() && client()->shouldInsertText(text, range, action);
}

void Editor::respondToChangedContents(const VisibleSelection& endingSelection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        RefPtr node = endingSelection.start().deprecatedNode();
        if (AXObjectCache* cache = document().existingAXObjectCache())
            cache->postNotification(node.get(), AXNotification::ValueChanged, PostTarget::ObservableParent);
    }

    updateMarkersForWordsAffectedByEditing(true);

    if (client())
        client()->respondToChangedContents();
}

bool Editor::hasBidiSelection() const
{
    Ref document = this->document();
    if (document->selection().isNone())
        return false;

    RefPtr<Node> startNode;
    if (document->selection().isRange()) {
        startNode = document->selection().selection().start().downstream().deprecatedNode();
        RefPtr endNode = document->selection().selection().end().upstream().deprecatedNode();
        if (enclosingBlock(startNode.get()) != enclosingBlock(endNode.get()))
            return false;
    } else
        startNode = document->selection().selection().visibleStart().deepEquivalent().deprecatedNode();

    if (!startNode || !startNode->renderer())
        return false;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (CheckedPtr renderBlockFlow = ancestorsOfType<RenderBlockFlow>(*startNode->renderer()).first())
        return !renderBlockFlow->style().isLeftToRightDirection() || renderBlockFlow->containsNonZeroBidiLevel();
    return false;
}

TriState Editor::selectionUnorderedListState() const
{
    Ref document = this->document();
    if (document->selection().isCaret()) {
        if (enclosingElementWithTag(document->selection().selection().start(), ulTag))
            return TriState::True;
    } else if (document->selection().isRange()) {
        RefPtr startNode { enclosingElementWithTag(document->selection().selection().start(), ulTag) };
        RefPtr endNode { enclosingElementWithTag(document->selection().selection().end(), ulTag) };
        if (startNode && endNode && startNode == endNode)
            return TriState::True;
    }

    return TriState::False;
}

TriState Editor::selectionOrderedListState() const
{
    Ref document = this->document();
    if (document->selection().isCaret()) {
        if (enclosingElementWithTag(document->selection().selection().start(), olTag))
            return TriState::True;
    } else if (document->selection().isRange()) {
        RefPtr startNode { enclosingElementWithTag(document->selection().selection().start(), olTag) };
        RefPtr endNode { enclosingElementWithTag(document->selection().selection().end(), olTag) };
        if (startNode && endNode && startNode == endNode)
            return TriState::True;
    }

    return TriState::False;
}

RefPtr<Node> Editor::insertOrderedList()
{
    if (!canEditRichly())
        return nullptr;
        
    auto newList = InsertListCommand::insertList(protectedDocument(), InsertListCommand::Type::OrderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::insertUnorderedList()
{
    if (!canEditRichly())
        return nullptr;
        
    auto newList = InsertListCommand::insertList(protectedDocument(), InsertListCommand::Type::UnorderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

bool Editor::canIncreaseSelectionListLevel()
{
    return canEditRichly() && IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(&document());
}

bool Editor::canDecreaseSelectionListLevel()
{
    return canEditRichly() && DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(&document());
}

RefPtr<Node> Editor::increaseSelectionListLevel()
{
    Ref document = this->document();
    if (!canEditRichly() || document->selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(document.ptr());
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::increaseSelectionListLevelOrdered()
{
    Ref document = this->document();
    if (!canEditRichly() || document->selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(document.ptr());
    revealSelectionAfterEditingOperation();
    return newList;
}

RefPtr<Node> Editor::increaseSelectionListLevelUnordered()
{
    Ref document = this->document();
    if (!canEditRichly() || document->selection().isNone())
        return nullptr;
    
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(document.ptr());
    revealSelectionAfterEditingOperation();
    return newList;
}

void Editor::decreaseSelectionListLevel()
{
    Ref document = this->document();
    if (!canEditRichly() || document->selection().isNone())
        return;
    
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(document.ptr());
    revealSelectionAfterEditingOperation();
}

void Editor::removeFormattingAndStyle()
{
    RemoveFormatCommand::create(protectedDocument())->apply();
}

void Editor::clearLastEditCommand() 
{
    m_lastEditCommand = nullptr;
}

RefPtr<Element> Editor::findEventTargetFrom(const VisibleSelection& selection) const
{
    RefPtr target { selection.start().anchorElementAncestor() };
    if (!target)
        target = document().bodyOrFrameset();

    return target;
}

RefPtr<Element> Editor::findEventTargetFromSelection() const
{
    // https://www.w3.org/TR/clipboard-apis/#fire-a-clipboard-event says:
    // If the context is editable, then
    // Set target to be the element that contains the start of the visible selection or cursor in document order, or the body element if there is no visible selection or cursor.
    const VisibleSelection& selection = document().selection().selection();
    if (selection.isRange() || isEditablePosition(selection.start()))
        return findEventTargetFrom(selection);
    // Else, if the context is not editable, then
    // Set target to the focused node, or the body element if no node has focus.
    return document().activeElement();
}

void Editor::applyStyle(StyleProperties* style, EditAction editingAction)
{
    if (style)
        applyStyle(EditingStyle::create(style), editingAction, ColorFilterMode::UseOriginalColor);
}

void Editor::applyStyle(RefPtr<EditingStyle>&& style, EditAction editingAction, ColorFilterMode colorFilterMode)
{
    if (!style)
        return;

    Ref document = this->document();
    if (document->selection().isNone())
        return;

    AtomString inputTypeName = inputTypeNameForEditingAction(editingAction);
    auto isInputMethodComposing = isInputMethodComposingForEditingAction(editingAction) ? IsInputMethodComposing::Yes : IsInputMethodComposing::No;
    String inputEventData = inputEventDataForEditingStyleAndAction(*style, editingAction);
    RefPtr element { document->selection().selection().rootEditableElement() };

    if (element && !dispatchBeforeInputEvent(*element, inputTypeName, isInputMethodComposing, inputEventData))
        return;

    if (document->selection().isNone())
        return;

    auto styleToApply = colorFilterMode == ColorFilterMode::InvertColor ? style->inverseTransformColorIfNeeded(*element) : style.releaseNonNull();
    if (document->selection().isCaret())
        computeAndSetTypingStyle(styleToApply, editingAction);
    else
        ApplyStyleCommand::create(WTFMove(document), styleToApply.ptr(), editingAction)->apply();

    if (client())
        client()->didApplyStyle();
    if (element)
        dispatchInputEvent(*element, inputTypeName, isInputMethodComposing, inputEventData);
}
    
bool Editor::shouldApplyStyle(const StyleProperties& style, const SimpleRange& range)
{   
    return client()->shouldApplyStyle(style, range);
}
    
void Editor::applyParagraphStyle(StyleProperties* style, EditAction editingAction)
{
    if (!style)
        return;

    Ref document = this->document();
    if (document->selection().isNone())
        return;

    AtomString inputTypeName = inputTypeNameForEditingAction(editingAction);
    auto isInputMethodComposing = isInputMethodComposingForEditingAction(editingAction) ? IsInputMethodComposing::Yes : IsInputMethodComposing::No;
    String inputEventData = inputEventDataForEditingStyleAndAction(style, editingAction);
    RefPtr element { document->selection().selection().rootEditableElement() };
    if (element && !dispatchBeforeInputEvent(*element, inputTypeName, isInputMethodComposing, inputEventData))
        return;
    if (document->selection().isNone())
        return;

    ApplyStyleCommand::create(WTFMove(document), EditingStyle::create(style).ptr(), editingAction, ApplyStylePropertyLevel::ForceBlock)->apply();

    if (client())
        client()->didApplyStyle();
    if (element)
        dispatchInputEvent(*element, inputTypeName, isInputMethodComposing, inputEventData);
}

void Editor::applyStyleToSelection(StyleProperties* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    if (!client() || !client()->shouldApplyStyle(*style, document().selection().selection().toNormalizedRange()))
        return;
    applyStyle(style, editingAction);
}

void Editor::applyStyleToSelection(Ref<EditingStyle>&& style, EditAction editingAction, ColorFilterMode colorFilterMode)
{
    if (style->isEmpty() || !canEditRichly())
        return;

    // FIXME: This is wrong for text decorations since m_mutableStyle is empty.
    if (!client() || !client()->shouldApplyStyle(style->styleWithResolvedTextDecorations(), document().selection().selection().toNormalizedRange()))
        return;

    applyStyle(WTFMove(style), editingAction, colorFilterMode);
}

void Editor::applyParagraphStyleToSelection(StyleProperties* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;
    
    if (client() && client()->shouldApplyStyle(*style, document().selection().selection().toNormalizedRange()))
        applyParagraphStyle(style, editingAction);
}

bool Editor::selectionStartHasStyle(CSSPropertyID propertyID, const String& value) const
{
    if (auto editingStyle = EditingStyle::styleAtSelectionStart(document().selection().selection(), propertyID == CSSPropertyBackgroundColor))
        return editingStyle->hasStyle(propertyID, value);
    return false;
}

TriState Editor::selectionHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(document().selection().selection());
}

String Editor::selectionStartCSSPropertyValue(CSSPropertyID propertyID)
{
    Ref document = this->document();
    RefPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(document->selection().selection(),
        propertyID == CSSPropertyBackgroundColor);
    if (!selectionStyle || !selectionStyle->style())
        return String();

    if (propertyID == CSSPropertyFontSize)
        return String::number(selectionStyle->legacyFontSize(document));
    return selectionStyle->style()->getPropertyValue(propertyID);
}

static void notifyTextFromControls(Element* startRoot, Element* endRoot, bool wasUserEdit = true)
{
    RefPtr startingTextControl { enclosingTextFormControl(firstPositionInOrBeforeNode(startRoot)) };
    RefPtr endingTextControl { enclosingTextFormControl(firstPositionInOrBeforeNode(endRoot)) };
    if (startingTextControl)
        startingTextControl->didEditInnerTextValue(wasUserEdit);
    if (endingTextControl && startingTextControl != endingTextControl)
        endingTextControl->didEditInnerTextValue(wasUserEdit);
}

static inline bool shouldRemoveAutocorrectionIndicator(bool shouldConsiderApplyingAutocorrection, bool autocorrectionWasApplied, bool isAutocompletion)
{
#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
    bool shouldRemoveIndicator = shouldConsiderApplyingAutocorrection && !autocorrectionWasApplied;
#if PLATFORM(IOS_FAMILY)
    // On iOS, unlike macOS, autocorrection is applied as two separate text insertions. The first insertion is
    // an autocompletion.
    return shouldRemoveIndicator && !isAutocompletion;
#else // !PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(isAutocompletion);
    return shouldRemoveIndicator;
#endif // !PLATFORM(IOS_FAMILY)
#else // !HAVE(AUTOCORRECTION_ENHANCEMENTS)
    UNUSED_PARAM(shouldConsiderApplyingAutocorrection);
    UNUSED_PARAM(isAutocompletion);
#endif // !HAVE(AUTOCORRECTION_ENHANCEMENTS)
    return !autocorrectionWasApplied;
}

static inline bool didApplyAutocorrection(Document& document, AlternativeTextController& alternativeTextController)
{
#if HAVE(AUTOCORRECTION_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)
    bool autocorrectionWasApplied = alternativeTextController.applyAutocorrectionBeforeTypingIfAppropriate();

    // On iOS, unlike macOS, autocorrection is applied as two separate text insertions: the correction
    // itself, followed by a space. This logic detects that an autocorrection was applied after the
    // space has been inserted.

    auto selection = document.selection().selection();
    auto startOfSelection = selection.start();

    auto wordStart = startOfWord(startOfSelection, WordSide::LeftWordIfOnBoundary);
    auto wordEnd = endOfWord(startOfSelection, WordSide::LeftWordIfOnBoundary);

    if (auto range = makeSimpleRange(wordStart, wordEnd)) {
        if (CheckedPtr markers = document.markersIfExists(); markers && markers->hasMarkers(*range, DocumentMarkerType::CorrectionIndicator))
            autocorrectionWasApplied = true;
    }

    return autocorrectionWasApplied;
#else
    UNUSED_PARAM(document);
    return alternativeTextController.applyAutocorrectionBeforeTypingIfAppropriate();
#endif
}

static inline void adjustMarkerTypesToRemoveForWordsAffectedByEditing(OptionSet<DocumentMarkerType>& markerTypes)
{
#if HAVE(AUTOCORRECTION_ENHANCEMENTS) && PLATFORM(IOS_FAMILY)
    markerTypes.remove(DocumentMarkerType::CorrectionIndicator);
#else
    UNUSED_PARAM(markerTypes);
#endif
}

static bool dispatchBeforeInputEvents(RefPtr<Element> startRoot, RefPtr<Element> endRoot, const AtomString& inputTypeName, IsInputMethodComposing isInputMethodComposing,
    const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { }, Event::IsCancelable cancelable = Event::IsCancelable::Yes)
{
    bool continueWithDefaultBehavior = true;
    if (startRoot)
        continueWithDefaultBehavior &= dispatchBeforeInputEvent(*startRoot, inputTypeName, isInputMethodComposing, data, WTFMove(dataTransfer), targetRanges, cancelable);
    if (endRoot && endRoot != startRoot)
        continueWithDefaultBehavior &= dispatchBeforeInputEvent(*endRoot, inputTypeName, isInputMethodComposing, data, WTFMove(dataTransfer), targetRanges, cancelable);
    return continueWithDefaultBehavior;
}

static void dispatchInputEvents(RefPtr<Element> startRoot, RefPtr<Element> endRoot, const AtomString& inputTypeName, IsInputMethodComposing isInputMethodComposing,
    const String& data = { }, RefPtr<DataTransfer>&& dataTransfer = nullptr, const Vector<RefPtr<StaticRange>>& targetRanges = { })
{
    if (startRoot)
        dispatchInputEvent(*startRoot, inputTypeName, isInputMethodComposing, data, WTFMove(dataTransfer), targetRanges);
    if (endRoot && endRoot != startRoot)
        dispatchInputEvent(*endRoot, inputTypeName, isInputMethodComposing, data, WTFMove(dataTransfer), targetRanges);
}

bool Editor::willApplyEditing(CompositeEditCommand& command, Vector<RefPtr<StaticRange>>&& targetRanges)
{
#if ENABLE(WRITING_TOOLS)
    if (suppressEditingForWritingTools()) {
        RELEASE_LOG(Editing, "Editor %p suppressed editing for Writing Tools", this);
        return false;
    }
#endif

    m_hasHandledAnyEditing = true;

    if (!command.shouldDispatchInputEvents())
        return true;

    RefPtr composition { command.composition() };
    if (!composition)
        return true;

    if (command.isTopLevelCommand() && command.isTypingCommand() && document().view())
        m_prohibitScrollingDueToContentSizeChangesWhileTyping = document().view()->prohibitScrollingWhenChangingContentSizeForScope();

    auto isInputMethodComposing = command.isInputMethodComposing() ? IsInputMethodComposing::Yes : IsInputMethodComposing::No;
    return dispatchBeforeInputEvents(composition->startingRootEditableElement(), composition->endingRootEditableElement(), command.inputEventTypeName(), isInputMethodComposing,
        command.inputEventData(), command.inputEventDataTransfer(), targetRanges, command.isBeforeInputEventCancelable() ? Event::IsCancelable::Yes : Event::IsCancelable::No);
}

void Editor::appliedEditing(CompositeEditCommand& command)
{
    LOG(Editing, "Editor %p appliedEditing", this);

    Ref document = this->document();
    document->updateLayout();

    ASSERT(command.composition());
    Ref composition = *command.composition();
    VisibleSelection newSelection(command.endingSelection());

    bool wasUserEdit = [&command] {
        RefPtr typingCommand = dynamicDowncast<TypingCommand>(command);
        return !typingCommand || !typingCommand->triggeringEventIsUntrusted();
    }();
    notifyTextFromControls(composition->startingRootEditableElement(), composition->endingRootEditableElement(), wasUserEdit);

    if (command.isTopLevelCommand()) {
        // Don't clear the typing style with this selection change. We do those things elsewhere if necessary.
        OptionSet<FrameSelection::SetSelectionOption> options;
        if (command.isDictationCommand())
            options.add(FrameSelection::SetSelectionOption::DictationTriggered);

        changeSelectionAfterCommand(newSelection, options);
    }

    auto isInputMethodComposing = command.isInputMethodComposing() ? IsInputMethodComposing::Yes : IsInputMethodComposing::No;
    if (command.shouldDispatchInputEvents()) {
        dispatchInputEvents(composition->startingRootEditableElement(), composition->endingRootEditableElement(), command.inputEventTypeName(), isInputMethodComposing,
            command.inputEventData(), command.inputEventDataTransfer());
    }

    if (command.isTopLevelCommand()) {
        updateEditorUINowIfScheduled();

        m_alternativeTextController->respondToAppliedEditing(&command);

        if (!command.preservesTypingStyle())
            document->selection().clearTypingStyle();

        // Command will be equal to last edit command only in the case of typing
        if (m_lastEditCommand.get() == &command)
            ASSERT(command.isTypingCommand());
        else {
            // Only register a new undo command if the command passed in is
            // different from the last command
            m_lastEditCommand = command;
            if (client())
                client()->registerUndoStep(m_lastEditCommand->ensureComposition());
        }
        respondToChangedContents(newSelection);

        if (command.isTypingCommand())
            m_prohibitScrollingDueToContentSizeChangesWhileTyping = nullptr;
    }
}

bool Editor::willUnapplyEditing(const EditCommandComposition& composition) const
{
    TypingCommand::closeTyping(protectedDocument());
    return dispatchBeforeInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyUndo"_s, IsInputMethodComposing::No);
}

void Editor::unappliedEditing(EditCommandComposition& composition)
{
    protectedDocument()->updateLayout();

    notifyTextFromControls(composition.startingRootEditableElement(), composition.endingRootEditableElement());

    VisibleSelection newSelection(composition.startingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::defaultSetSelectionOptions());
    dispatchInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyUndo"_s, IsInputMethodComposing::No);

    updateEditorUINowIfScheduled();

    m_alternativeTextController->respondToUnappliedEditing(&composition);
#if ENABLE(WRITING_TOOLS)
    protectedDocument()->page()->respondToUnappliedWritingToolsEditing(&composition);
#endif

    m_lastEditCommand = nullptr;
    if (auto* client = this->client())
        client->registerRedoStep(composition);
    respondToChangedContents(newSelection);
}

bool Editor::willReapplyEditing(const EditCommandComposition& composition) const
{
    return dispatchBeforeInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyRedo"_s, IsInputMethodComposing::No);
}

void Editor::reappliedEditing(EditCommandComposition& composition)
{
    protectedDocument()->updateLayout();

    notifyTextFromControls(composition.startingRootEditableElement(), composition.endingRootEditableElement());

    VisibleSelection newSelection(composition.endingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::defaultSetSelectionOptions());
    dispatchInputEvents(composition.startingRootEditableElement(), composition.endingRootEditableElement(), "historyRedo"_s, IsInputMethodComposing::No);
    
    updateEditorUINowIfScheduled();

#if ENABLE(WRITING_TOOLS)
    if (RefPtr page = document().page())
        page->respondToReappliedWritingToolsEditing(&composition);
#endif

    m_lastEditCommand = nullptr;
    if (auto* client = this->client())
        client->registerUndoStep(composition);
    respondToChangedContents(newSelection);
}

Editor::Editor(Document& document)
    : m_client(document.page() ? &document.page()->editorClient() : nullptr)
    , m_document(document)
    , m_killRing(makeUniqueRef<PAL::KillRing>())
    , m_spellChecker(makeUniqueRefWithoutRefCountedCheck<SpellChecker>(*this))
    , m_alternativeTextController(makeUniqueRef<AlternativeTextController>(document))
    , m_editorUIUpdateTimer(*this, &Editor::editorUIUpdateTimerFired)
#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS_FAMILY)
    , m_telephoneNumberDetectionUpdateTimer(*this, &Editor::scanSelectionForTelephoneNumbers, 0_s)
#endif
{
}

Editor::~Editor() = default;

void Editor::ref() const
{
    m_document->ref();
}

void Editor::deref() const
{
    m_document->deref();
}

void Editor::clear()
{
    m_lastEditCommand = nullptr;
    if (m_compositionNode) {
        m_compositionNode = nullptr;
        if (EditorClient* client = this->client())
            client->discardedComposition(protectedDocument());
    }
    m_customCompositionUnderlines.clear();
    m_customCompositionHighlights.clear();
    m_customCompositionAnnotations.clear();
    m_shouldStyleWithCSS = false;
    m_defaultParagraphSeparator = EditorParagraphSeparator::div;
    m_mark = { };
    m_oldSelectionForEditorUIUpdate = { };
    m_editorUIUpdateTimer.stop();
    m_alternativeTextController->stopAlternativeTextUITimer();

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS_FAMILY)
    m_telephoneNumberDetectionUpdateTimer.stop();
    m_detectedTelephoneNumberRanges.clear();
#endif
}

bool Editor::insertText(const String& text, Event* triggeringEvent, TextEventInputType inputType)
{
    return document().frame()->eventHandler().handleTextInputEvent(text, triggeringEvent, inputType);
}

bool Editor::insertTextForConfirmedComposition(const String& text)
{
    return document().frame()->eventHandler().handleTextInputEvent(text, 0, TextEventInputComposition);
}

bool Editor::insertDictatedText(const String& text, const Vector<DictationAlternative>& dictationAlternatives, Event* triggeringEvent)
{
    return m_alternativeTextController->insertDictatedText(text, dictationAlternatives, triggeringEvent);
}

bool Editor::insertTextWithoutSendingTextEvent(const String& text, bool selectInsertedText, TextEvent* triggeringEvent)
{
    if (text.isEmpty())
        return false;

    VisibleSelection selection = selectionForCommand(triggeringEvent);
    if (!selection.isContentEditable())
        return false;

    if (!shouldInsertText(text, selection.toNormalizedRange(), EditorInsertAction::Typed))
        return true;

    // FIXME: Should pass false to updateMarkersForWordsAffectedByEditing() to not remove markers if
    // a leading or trailing no-break space is being inserted. See <https://webkit.org/b/212098>.
    bool isStartOfNewWord = deprecatedIsSpaceOrNewline(selection.visibleStart().characterBefore());
    updateMarkersForWordsAffectedByEditing(deprecatedIsSpaceOrNewline(text[0]) || isStartOfNewWord);

    bool shouldConsiderApplyingAutocorrection = false;
    if (text == " "_s || text == "\t"_s)
        shouldConsiderApplyingAutocorrection = true;

    if (text.length() == 1 && u_ispunct(text[0]) && !isAmbiguousBoundaryCharacter(text[0]))
        shouldConsiderApplyingAutocorrection = true;

    bool autocorrectionWasApplied = shouldConsiderApplyingAutocorrection && didApplyAutocorrection(document(), m_alternativeTextController);

    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    selection = selectionForCommand(triggeringEvent);
    if (selection.isContentEditable()) {
        if (RefPtr selectionStart = selection.start().deprecatedNode()) {
            Ref document = selectionStart->document();

            // Insert the text
            if (triggeringEvent && triggeringEvent->isDictation())
                DictationCommand::insertText(document.copyRef(), text, triggeringEvent->dictationAlternatives(), selection);
            else {
                auto options = OptionSet { TypingCommand::Option::RetainAutocorrectionIndicator };
                if (selectInsertedText)
                    options.add(TypingCommand::Option::SelectInsertedText);
                if (triggeringEvent && triggeringEvent->isAutocompletion())
                    options.add(TypingCommand::Option::IsAutocompletion);
                if (shouldRemoveAutocorrectionIndicator(shouldConsiderApplyingAutocorrection, autocorrectionWasApplied, options.contains(TypingCommand::Option::IsAutocompletion)))
                    options.remove(TypingCommand::Option::RetainAutocorrectionIndicator);
                auto compositionType = triggeringEvent && triggeringEvent->isComposition() ? TypingCommand::TextCompositionType::Final : TypingCommand::TextCompositionType::None;
                TypingCommand::insertText(document.copyRef(), text, triggeringEvent, selection, options, compositionType);
            }

            // Reveal the current selection. Note that focus may have changed after insertion.
            // FIXME: Selection is allowed even if setIgnoreSelectionChanges(true). Ideally setIgnoreSelectionChanges()
            // should be moved from Editor to a page-level like object. If it must remain a frame-specific concept
            // then this code should conditionalize revealing selection on whether the ignoreSelectionChanges() bit
            // is set for the newly focused frame.
            if ((!triggeringEvent || triggeringEvent->isTrusted()) && client() && client()->shouldRevealCurrentSelectionAfterInsertion()) {
                if (RefPtr page = document->page())
                    page->revealCurrentSelection();
            }
        }
    }

    return true;
}

bool Editor::insertLineBreak()
{
    if (!canEdit())
        return false;

    Ref document = this->document();
    if (!shouldInsertText("\n"_s, document->selection().selection().toNormalizedRange(), EditorInsertAction::Typed))
        return true;

    VisiblePosition caret = document->selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    bool autocorrectionIsApplied = m_alternativeTextController->applyAutocorrectionBeforeTypingIfAppropriate();
    TypingCommand::insertLineBreak(WTFMove(document), autocorrectionIsApplied ? TypingCommand::Option::RetainAutocorrectionIndicator : OptionSet<TypingCommand::Option> { });
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparator()
{
    if (!canEdit())
        return false;

    if (!canEditRichly())
        return insertLineBreak();
    
    Ref document = this->document();
    if (!shouldInsertText("\n"_s, document->selection().selection().toNormalizedRange(), EditorInsertAction::Typed))
        return true;

    VisiblePosition caret = document->selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    bool autocorrectionIsApplied = m_alternativeTextController->applyAutocorrectionBeforeTypingIfAppropriate();
    TypingCommand::insertParagraphSeparator(WTFMove(document), autocorrectionIsApplied ? TypingCommand::Option::RetainAutocorrectionIndicator : OptionSet<TypingCommand::Option> { });
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparatorInQuotedContent()
{
    // FIXME: Why is this missing calls to canEdit, canEditRichly, etc.?
    TypingCommand::insertParagraphSeparatorInQuotedContent(protectedDocument());
    revealSelectionAfterEditingOperation();
    return true;
}

void Editor::cut(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope copyScope { m_copyingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        SystemSoundManager::singleton().systemBeep();
        return;
    }

    performCutOrCopy(CutAction);
}

void Editor::copy(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope copyScope { m_copyingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        SystemSoundManager::singleton().systemBeep();
        return;
    }

    performCutOrCopy(CopyAction);
}

void Editor::copyFont(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope copyScope { m_copyingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopyFont()) {
        SystemSoundManager::singleton().systemBeep();
        return;
    }

    willWriteSelectionToPasteboard(selectedRange());
    platformCopyFont();
    didWriteSelectionToPasteboard();
}

bool Editor::canCopyFont() const
{
    auto& selection = document().selection().selection();
    if (selection.isNone() || (selection.isInPasswordField() && !selection.isInAutoFilledAndViewableField()))
        return false;
    return selection.isRange() || selection.isContentEditable();
}

void Editor::postTextStateChangeNotificationForCut(const String& text, const VisibleSelection& selection)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;
    if (!text.length())
        return;
    CheckedPtr cache = document().existingAXObjectCache();
    if (!cache)
        return;
    cache->postTextStateChangeNotification(selection.start().anchorNode(), AXTextEditTypeCut, text, selection.start());
}

void Editor::performCutOrCopy(EditorActionSpecifier action)
{
    auto selection = selectedRange();
    willWriteSelectionToPasteboard(selection);
    if (action == CutAction) {
        if (!shouldDeleteRange(selection))
            return;

        updateMarkersForWordsAffectedByEditing(true);
    }

    Ref document = this->document();
    if (enclosingTextFormControl(document->selection().selection().start()))
        Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID()))->writePlainText(selectedTextForDataTransfer(), canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
    else {
        RefPtr<HTMLImageElement> imageElement;
        bool isSelectionInImageOverlay = selection && !selection->collapsed() && ImageOverlay::isInsideOverlay(*selection);
        if (action == CopyAction && !isSelectionInImageOverlay)
            imageElement = imageElementFromImageDocument(document);

        if (imageElement) {
#if !PLATFORM(WIN)
            writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID())), *imageElement, document->url(), document->title());
#else
            // FIXME: Delete after <http://webkit.org/b/177618> lands.
            Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID()))->writeImage(*imageElement, document->url(), document->title());
#endif
        } else {
#if !PLATFORM(WIN)
            writeSelectionToPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID())));
#else
            // FIXME: Delete after <http://webkit.org/b/177618> lands.
            Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID()))->writeSelection(selection, canSmartCopyOrDelete(), *document->frame(), IncludeImageAltTextForDataTransfer);
#endif
        }
    }

    didWriteSelectionToPasteboard();
    if (action == CutAction) {
        String text;
        if (AXObjectCache::accessibilityEnabled())
            text = AccessibilityObject::stringForVisiblePositionRange(document->selection().selection());
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete(), EditAction::Cut);
        if (AXObjectCache::accessibilityEnabled())
            postTextStateChangeNotificationForCut(text, document->selection().selection());
    }
}

void Editor::paste(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    paste(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID())), fromMenuOrKeyBinding);
}

void Editor::paste(Pasteboard& pasteboard, FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope pasteScope { m_pastingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };

    if (!dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::Paste))
        return; // DHTML did the whole operation
    if (!canEdit())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    ResourceCacheValidationSuppressor validationSuppressor(document().cachedResourceLoader());
    if (document().selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(&pasteboard, { PasteOption::AllowPlainText });
    else
        pasteAsPlainTextWithPasteboard(pasteboard);
}

void Editor::pasteAsPlainText(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope pasteScope { m_pastingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };

    if (!dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::PasteAsPlainText))
        return;
    if (!canEdit())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    pasteAsPlainTextWithPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID())));
}

void Editor::pasteAsQuotation(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope pasteScope { m_pastingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };

    if (!dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::PasteAsQuotation))
        return;
    if (!canEdit())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    Ref document = this->document();
    ResourceCacheValidationSuppressor validationSuppressor(document->cachedResourceLoader());
    auto pasteboard = Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document->pageID()));
    if (document->selection().selection().isContentRichlyEditable())
        pasteWithPasteboard(pasteboard.get(), { PasteOption::AllowPlainText, PasteOption::AsQuotation });
    else
        pasteAsPlainTextWithPasteboard(*pasteboard);
}

void Editor::pasteFont(FromMenuOrKeyBinding fromMenuOrKeyBinding)
{
    SetForScope pasteScope { m_pastingFromMenuOrKeyBinding, fromMenuOrKeyBinding == FromMenuOrKeyBinding::Yes };

    if (!dispatchClipboardEvent(findEventTargetFromSelection(), ClipboardEventKind::PasteFont))
        return;
    if (!canEdit())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    ResourceCacheValidationSuppressor validationSuppressor(document().cachedResourceLoader());
    platformPasteFont();
}

void Editor::quoteFragmentForPasting(DocumentFragment& fragment)
{
    Ref document = this->document();
    auto blockQuote = HTMLQuoteElement::create(blockquoteTag, document);
    blockQuote->setAttributeWithoutSynchronization(typeAttr, "cite"_s);
    blockQuote->setAttributeWithoutSynchronization(classAttr, ApplePasteAsQuotation);

    RefPtr childNode = fragment.firstChild();

    if (childNode) {
        while (childNode) {
            blockQuote->appendChild(*childNode);
            childNode = fragment.firstChild();
        }
    } else
        blockQuote->appendChild(HTMLBRElement::create(document));

    fragment.appendChild(blockQuote);
}

void Editor::performDelete()
{
    if (!canDelete()) {
        SystemSoundManager::singleton().systemBeep();
        return;
    }

    addRangeToKillRing(*selectedRange(), KillRingInsertionMode::AppendText);
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    setStartNewKillRingSequence(false);
}

void Editor::changeSelectionListType()
{
    Ref document = this->document();
    if (auto type = ChangeListTypeCommand::listConversionType(document))
        ChangeListTypeCommand::create(WTFMove(document), *type)->apply();
}

void Editor::simplifyMarkup(Node* startNode, Node* endNode)
{
    if (!startNode)
        return;

    RefPtr<Node> pastEndNode;
    if (endNode) {
        if (!is_lt(treeOrder(*startNode, *endNode)))
            return;
        pastEndNode = NodeTraversal::next(*endNode);
    }
    
    SimplifyMarkupCommand::create(protectedDocument(), startNode, pastEndNode.get())->apply();
}

void Editor::copyURL(const URL& url, const String& title)
{
    copyURL(url, title, *Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID())));
}

void Editor::copyURL(const URL& url, const String& title, Pasteboard& pasteboard)
{
    auto sanitizedURL = url;
    if (RefPtr page = document().page())
        sanitizedURL = page->applyLinkDecorationFiltering(url, LinkDecorationFilteringTrigger::Copy);

    PasteboardURL pasteboardURL;
    pasteboardURL.url = sanitizedURL;
    pasteboardURL.title = title;

#if PLATFORM(MAC)
    pasteboardURL.userVisibleForm = userVisibleString(sanitizedURL);
#endif

    pasteboard.write(pasteboardURL);
}

PasteboardWriterData::URLData Editor::pasteboardWriterURL(const URL& url, const String& title)
{
    PasteboardWriterData::URLData result;

    result.url = url;
    result.title = title;
#if PLATFORM(MAC)
    result.userVisibleForm = userVisibleString(url);
#endif

    return result;
}

#if !PLATFORM(IOS_FAMILY)

void Editor::copyImage(const HitTestResult& result)
{
    RefPtr element { result.innerNonSharedElement() };
    if (!element)
        return;

    URL url = result.absoluteLinkURL();
    if (url.isEmpty())
        url = result.absoluteImageURL();

#if !PLATFORM(WIN)
    writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID())), *element, url, result.altDisplayString());
#else
    // FIXME: Delete after <http://webkit.org/b/177618> lands.
    Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(document().pageID()))->writeImage(*element, url, result.altDisplayString());
#endif
}

#endif

void Editor::revealSelectionIfNeededAfterLoadingImageForElement(HTMLImageElement& element)
{
    if (m_imageElementsToLoadBeforeRevealingSelection.isEmpty())
        return;

    if (!m_imageElementsToLoadBeforeRevealingSelection.remove(&element))
        return;

    if (!m_imageElementsToLoadBeforeRevealingSelection.isEmpty())
        return;

    // FIXME: This should be queued as a task for the next rendering update.
    Ref document = this->document();
    document->updateLayout();
    document->selection().setCaretRectNeedsUpdate();
    revealSelectionAfterEditingOperation();
}

void Editor::renderLayerDidScroll(const RenderLayer& layer)
{
    if (m_imageElementsToLoadBeforeRevealingSelection.isEmpty())
        return;

    RefPtr startContainer { document().selection().selection().start().containerNode() };
    if (!startContainer)
        return;

    auto* startContainerRenderer = startContainer->renderer();
    if (!startContainerRenderer)
        return;

    // FIXME: Ideally, this would also cancel deferred selection revealing if the selection is inside a subframe and a parent frame is scrolled.
    for (auto* enclosingLayer = startContainerRenderer->enclosingLayer(); enclosingLayer; enclosingLayer = enclosingLayer->parent()) {
        if (enclosingLayer == &layer) {
            m_imageElementsToLoadBeforeRevealingSelection.clear();
            break;
        }
    }
}

bool Editor::isContinuousSpellCheckingEnabled() const
{
    return client() && client()->isContinuousSpellCheckingEnabled();
}

void Editor::toggleContinuousSpellChecking()
{
    if (client())
        client()->toggleContinuousSpellChecking();
}

bool Editor::isGrammarCheckingEnabled()
{
    return client() && client()->isGrammarCheckingEnabled();
}

void Editor::toggleGrammarChecking()
{
    if (client())
        client()->toggleGrammarChecking();
}

int Editor::spellCheckerDocumentTag()
{
    return client() ? client()->spellCheckerDocumentTag() : 0;
}

#if USE(APPKIT)

void Editor::uppercaseWord()
{
    if (client())
        client()->uppercaseWord();
}

void Editor::lowercaseWord()
{
    if (client())
        client()->lowercaseWord();
}

void Editor::capitalizeWord()
{
    if (client())
        client()->capitalizeWord();
}
    
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)

void Editor::showSubstitutionsPanel()
{
    if (!client()) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    if (client()->substitutionsPanelIsShowing()) {
        client()->showSubstitutionsPanel(false);
        return;
    }
    client()->showSubstitutionsPanel(true);
}

bool Editor::substitutionsPanelIsShowing()
{
    if (!client())
        return false;
    return client()->substitutionsPanelIsShowing();
}

void Editor::toggleSmartInsertDelete()
{
    if (client())
        client()->toggleSmartInsertDelete();
}

bool Editor::isAutomaticQuoteSubstitutionEnabled()
{
    return client() && client()->isAutomaticQuoteSubstitutionEnabled();
}

void Editor::toggleAutomaticQuoteSubstitution()
{
    if (client())
        client()->toggleAutomaticQuoteSubstitution();
}

bool Editor::isAutomaticLinkDetectionEnabled()
{
    return client() && client()->isAutomaticLinkDetectionEnabled();
}

void Editor::toggleAutomaticLinkDetection()
{
    if (client())
        client()->toggleAutomaticLinkDetection();
}

bool Editor::isAutomaticDashSubstitutionEnabled()
{
    return client() && client()->isAutomaticDashSubstitutionEnabled();
}

void Editor::toggleAutomaticDashSubstitution()
{
    if (client())
        client()->toggleAutomaticDashSubstitution();
}

bool Editor::isAutomaticTextReplacementEnabled()
{
    return client() && client()->isAutomaticTextReplacementEnabled();
}

void Editor::toggleAutomaticTextReplacement()
{
    if (client())
        client()->toggleAutomaticTextReplacement();
}

bool Editor::canEnableAutomaticSpellingCorrection() const
{
    return m_alternativeTextController->canEnableAutomaticSpellingCorrection();
}

bool Editor::isAutomaticSpellingCorrectionEnabled()
{
    return m_alternativeTextController->isAutomaticSpellingCorrectionEnabled();
}

void Editor::toggleAutomaticSpellingCorrection()
{
    if (client())
        client()->toggleAutomaticSpellingCorrection();
}

#endif

bool Editor::shouldEndEditing(const SimpleRange& range)
{
    return client() && client()->shouldEndEditing(range);
}

bool Editor::shouldBeginEditing(const SimpleRange& range)
{
    return client() && client()->shouldBeginEditing(range);
}

void Editor::clearUndoRedoOperations()
{
    if (client())
        client()->clearUndoRedoOperations();
}

bool Editor::canUndo() const
{
    return client() && client()->canUndo();
}

void Editor::undo()
{
    if (client())
        client()->undo();
}

bool Editor::canRedo() const
{
    return client() && client()->canRedo();
}

void Editor::redo()
{
    if (client())
        client()->redo();
}

void Editor::registerCustomUndoStep(Ref<CustomUndoStep>&& undoStep)
{
    ASSERT(document().settings().undoManagerAPIEnabled());
    if (auto* client = this->client())
        client->registerUndoStep(WTFMove(undoStep));
}

void Editor::didBeginEditing()
{
    if (client())
        client()->didBeginEditing();
}

void Editor::didEndEditing()
{
    if (client())
        client()->didEndEditing();
}

void Editor::willWriteSelectionToPasteboard(const std::optional<SimpleRange>& range)
{
    if (client())
        client()->willWriteSelectionToPasteboard(range);
}

void Editor::didWriteSelectionToPasteboard()
{
    if (client())
        client()->didWriteSelectionToPasteboard();
}

void Editor::toggleBold()
{
    command("ToggleBold"_s).execute();
}

void Editor::toggleUnderline()
{
    command("ToggleUnderline"_s).execute();
}

void Editor::setTextAlignmentForChangedBaseWritingDirection(WritingDirection direction)
{
    // Note that the passed-in argument is the direction that has been changed to by
    // some code or user interaction outside the scope of this function. The former
    // direction is not known, nor is it required for the kind of text alignment
    // changes done by this function.
    //
    // Rules:
    // When text has no explicit alignment, set alignment to match the writing direction.
    // If the text has left or right alignment, flip left->right and right->left.
    // Otherwise, do nothing.

    Ref document = this->document();
    RefPtr selectionStyle = EditingStyle::styleAtSelectionStart(document->selection().selection());
    if (!selectionStyle || !selectionStyle->style())
        return;

    auto value = selectionStyle->style()->propertyAsValueID(CSSPropertyTextAlign);
    if (!value)
        return;

    auto newValue = CSSValueInvalid;
    switch (*value) {
    case CSSValueStart:
    case CSSValueEnd:
        switch (direction) {
        case WritingDirection::Natural:
            return;
        case WritingDirection::LeftToRight:
            newValue = CSSValueLeft;
            break;
        case WritingDirection::RightToLeft:
            newValue = CSSValueRight;
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }
        break;
    case CSSValueLeft:
    case CSSValueWebkitLeft:
        newValue = CSSValueRight;
        break;
    case CSSValueRight:
    case CSSValueWebkitRight:
        newValue = CSSValueLeft;
        break;
    case CSSValueCenter:
    case CSSValueWebkitCenter:
    case CSSValueJustify:
        return;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    auto isTextControl = [](Element* focusedElement) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(focusedElement))
            return input->isTextField() || input->isSearchField();
        return is<HTMLTextAreaElement>(focusedElement);
    };

    if (RefPtr focusedElement = document->focusedElement(); isTextControl(focusedElement.get())) {
        if (direction != WritingDirection::Natural) {
            focusedElement->setAttributeWithoutSynchronization(alignAttr, nameString(newValue));
            document->updateStyleIfNeeded();
        }
        return;
    }

    Ref style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyTextAlign, newValue);
    applyParagraphStyle(style.ptr());
}

void Editor::setBaseWritingDirection(WritingDirection direction)
{
    Ref document = this->document();
#if PLATFORM(IOS_FAMILY)
    if (inSameParagraph(document->selection().selection().visibleStart(), document->selection().selection().visibleEnd())
        && baseWritingDirectionForSelectionStart() == direction)
        return;
#endif

    if (RefPtr focusedElement = dynamicDowncast<HTMLTextFormControlElement>(document->focusedElement()); focusedElement && focusedElement->isTextField()) {
        if (direction == WritingDirection::Natural)
            return;

        auto directionValue = direction == WritingDirection::LeftToRight ? "ltr"_s : "rtl"_s;
        auto writingDirectionInputTypeName = inputTypeNameForEditingAction(EditAction::SetBlockWritingDirection);
        if (!dispatchBeforeInputEvent(*focusedElement, writingDirectionInputTypeName, IsInputMethodComposing::No, directionValue))
            return;

        focusedElement->setAttributeWithoutSynchronization(dirAttr, directionValue);
        dispatchInputEvent(*focusedElement, writingDirectionInputTypeName, IsInputMethodComposing::No, directionValue);
        document->updateStyleIfNeeded();
        return;
    }

    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyDirection, direction == WritingDirection::LeftToRight ? "ltr"_s : direction == WritingDirection::RightToLeft ? "rtl"_s : "inherit"_s);
    applyParagraphStyleToSelection(style.ptr(), EditAction::SetBlockWritingDirection);
}

WritingDirection Editor::baseWritingDirectionForSelectionStart() const
{
    auto result = WritingDirection::LeftToRight;

    Position pos = document().selection().selection().visibleStart().deepEquivalent();
    RefPtr node = pos.deprecatedNode();
    if (!node)
        return result;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    CheckedPtr renderer = node->renderer();
    if (renderer && !is<RenderBlock>(*renderer))
        renderer = renderer->containingBlock();
    if (!renderer)
        return result;

    switch (downcast<RenderBlock>(*renderer).writingMode().bidiDirection()) {
    case TextDirection::LTR:
        return WritingDirection::LeftToRight;
    case TextDirection::RTL:
        return WritingDirection::RightToLeft;
    }
    
    return result;
}

void Editor::selectComposition()
{
    auto range = compositionRange();
    if (!range)
        return;
    
    // The composition can start inside a composed character sequence, so we have to override checks.
    // See <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection selection;
    selection.setWithoutValidation(makeDeprecatedLegacyPosition(range->start), makeDeprecatedLegacyPosition(range->end));
    document().selection().setSelection(selection, { });
}

Node* Editor::nodeBeforeWritingSuggestions() const
{
    Ref document = this->document();
    if (!document->selection().isCaret())
        return nullptr;

    auto position = document->selection().selection().end();
    RefPtr container = position.containerNode();
    if (!container)
        return nullptr;

    if (RefPtr text = dynamicDowncast<Text>(container))
        return text.get();

    return position.computeNodeBeforePosition();
}

Element* Editor::writingSuggestionsContainerElement() const
{
    RefPtr node = nodeBeforeWritingSuggestions();
    if (!node)
        return nullptr;

    return node->parentElement();
}

void Editor::removeWritingSuggestionIfNeeded()
{
    Ref document = this->document();
    document->updateStyleIfNeeded();

    m_customCompositionAnnotations = { };
    m_isHandlingAcceptedCandidate = false;

    RefPtr selectedElement = writingSuggestionsContainerElement();
    if (!selectedElement)
        return;

    m_writingSuggestionData = nullptr;
    selectedElement->invalidateStyleAndRenderersForSubtree();
}

void Editor::confirmComposition()
{
#if PLATFORM(MAC)
    if (m_isHandlingAcceptedCandidate) {
        removeWritingSuggestionIfNeeded();
        return;
    }
#endif

    if (!m_compositionNode)
        return;
    setComposition(m_compositionNode->data().substring(m_compositionStart, m_compositionEnd - m_compositionStart), ConfirmComposition);
}

void Editor::confirmOrCancelCompositionAndNotifyClient()
{
    if (!hasComposition())
        return;

    Ref document = this->document();
    RefPtr frame = document->frame();
    if (!frame)
        return;

    if (cancelCompositionIfSelectionIsInvalid())
        return;

    confirmComposition();

    if (auto editorClient = client()) {
        editorClient->respondToChangedSelection(frame.get());
        editorClient->discardedComposition(document);
    }
}

void Editor::cancelComposition()
{
#if PLATFORM(MAC)
    if (m_isHandlingAcceptedCandidate) {
        removeWritingSuggestionIfNeeded();
        return;
    }
#endif

    if (!m_compositionNode)
        return;
    setComposition(emptyString(), CancelComposition);
}

bool Editor::cancelCompositionIfSelectionIsInvalid()
{
    unsigned start;
    unsigned end;
    if (!hasComposition() || ignoreSelectionChanges() || getCompositionSelection(start, end))
        return false;

    cancelComposition();
    return true;
}

void Editor::confirmComposition(const String& text)
{
    setComposition(text, ConfirmComposition);
}

class SetCompositionScope {
public:
    SetCompositionScope(Ref<Document>&& document)
        : m_document(WTFMove(document))
        , m_typingGestureIndicator(*m_document->frame())
    {
        m_document->editor().setIgnoreSelectionChanges(true);
    }

    ~SetCompositionScope()
    {
        m_document->editor().setIgnoreSelectionChanges(false);
        if (auto* editorClient = m_document->editor().client())
            editorClient->didUpdateComposition();
    }

    RefPtr<Document> m_document;
    UserTypingGestureIndicator m_typingGestureIndicator;
};

void Editor::setComposition(const String& text, SetCompositionMode mode)
{
    ASSERT(mode == ConfirmComposition || mode == CancelComposition);
    Ref document = this->document();
    SetCompositionScope setCompositionScope(document.copyRef());

    if (mode == CancelComposition)
        ASSERT(text == emptyString());
    else
        selectComposition();

    RefPtr previousCompositionNode = m_compositionNode;
    m_compositionNode = nullptr;
    m_customCompositionUnderlines.clear();
    m_customCompositionHighlights.clear();
    m_customCompositionAnnotations.clear();

    if (CheckedPtr cache = document->existingAXObjectCache(); cache && previousCompositionNode)
        cache->onTextCompositionChange(*previousCompositionNode, CompositionState::Ended, false, text, m_compositionStart, m_isHandlingAcceptedCandidate);

    if (document->selection().isNone())
        return;

    // Always delete the current composition before inserting the finalized composition text if we're confirming our composition.
    // Our default behavior (if the beforeinput event is not prevented) is to insert the finalized composition text back in.
    // We pass TypingCommand::TextCompositionType::Pending here to indicate that we are deleting the pending composition.
    if (mode != CancelComposition)
        TypingCommand::deleteSelection(document.copyRef(), { }, TypingCommand::TextCompositionType::Pending);

    insertTextForConfirmedComposition(text);

    if (RefPtr target = document->focusedElement())
        target->dispatchEvent(CompositionEvent::create(eventNames().compositionendEvent, document->windowProxy(), text));

    if (mode == CancelComposition) {
        // An open typing command that disagrees about current selection would cause issues with typing later on.
        TypingCommand::closeTyping(document);
    }
}

void Editor::closeTyping()
{
    TypingCommand::closeTyping(m_document);
}

RenderInline* Editor::writingSuggestionRenderer() const
{
    return m_writingSuggestionRenderer.get();
}

void Editor::setWritingSuggestionRenderer(RenderInline& renderer)
{
    m_writingSuggestionRenderer = renderer;
}

#if PLATFORM(COCOA)
void Editor::setWritingSuggestion(const String& fullTextWithPrediction, const CharacterRange& selection)
{
    Ref document = this->document();
    document->updateStyleIfNeeded();

    RefPtr selectedElement = writingSuggestionsContainerElement();
    if (!selectedElement)
        return;

    if (!selectedElement->hasEditableStyle())
        return;

    auto range = document->selection().selection().firstRange();
    if (!range)
        return;

    if (!range->collapsed())
        return;

    if (!is<Text>(range->startContainer()))
        return;

    range->start.offset = 0;

    m_isHandlingAcceptedCandidate = true;

    auto newText = fullTextWithPrediction.substring(0, selection.location);
    auto suggestionText = fullTextWithPrediction.substring(selection.location);

    auto currentText = m_writingSuggestionData ? m_writingSuggestionData->currentText() : emptyString();

    ASSERT(newText.isEmpty() || newText.startsWith(currentText));
    auto textDelta = newText.isEmpty() ? emptyString() : newText.substring(currentText.length());

    auto offset = range->endOffset();
    auto offsetWithDelta = currentText.isEmpty() ? offset : offset + textDelta.length();

    if (!suggestionText.isEmpty()) {
        String originalPrefix;
        String originalSuffix;
        if (m_writingSuggestionData) {
            originalPrefix = m_writingSuggestionData->originalPrefix();
            originalSuffix = m_writingSuggestionData->originalSuffix();
        } else {
            originalPrefix = WebCore::plainTextReplacingNoBreakSpace(*range);
            originalSuffix = suggestionText;
        }

        m_writingSuggestionData = makeUnique<WritingSuggestionData>(WTFMove(suggestionText), WTFMove(newText), WTFMove(offsetWithDelta), WTFMove(originalPrefix), WTFMove(originalSuffix), Editor::writingSuggestionsSupportsSuffix());
    } else
        m_writingSuggestionData = nullptr;

    if (!currentText.isEmpty()) {
        SetForScope isInsertingTextForWritingSuggestionScope { m_isInsertingTextForWritingSuggestion, true };
        insertText(textDelta, nullptr, TextEventInputKeyboard);
    } else
        selectedElement->invalidateStyleAndRenderersForSubtree();
}
#endif

void Editor::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<CharacterRange>>& annotations, unsigned selectionStart, unsigned selectionEnd)
{
    Ref document = this->document();
    SetCompositionScope setCompositionScope(document.copyRef());

    // Updates styles before setting selection for composition to prevent
    // inserting the previous composition text into text nodes oddly.
    // See https://bugs.webkit.org/show_bug.cgi?id=46868
    document->updateStyleIfNeeded();

    selectComposition();

    if (document->selection().isNone())
        return;

    String originalText = selectedText();
    bool isStartingToRecomposeExistingRange = !text.isEmpty() && selectionStart < selectionEnd && !hasComposition();
    if (isStartingToRecomposeExistingRange) {
        // We pass TypingCommand::TextCompositionType::Final here to indicate that we are removing composition text that has been finalized.
        TypingCommand::deleteSelection(document.copyRef(), { }, TypingCommand::TextCompositionType::Final);
        const VisibleSelection& currentSelection = document->selection().selection();
        if (currentSelection.isRange()) {
            // If deletion was prevented, then we need to collapse the selection to the end so that the original text will not be recomposed.
            document->selection().setSelection({ currentSelection.end(), currentSelection.end() });
        }
    }

#if PLATFORM(IOS_FAMILY)
    client()->startDelayingAndCoalescingContentChangeNotifications();
#endif

    RefPtr<CompositionEvent> event;
    RefPtr target { document->focusedElement() };
    if (target) {
        // Dispatch an appropriate composition event to the focused node.
        // We check the composition status and choose an appropriate composition event since this
        // function is used for three purposes:
        // 1. Starting a new composition.
        //    Send a compositionstart and a compositionupdate event when this function creates
        //    a new composition node, i.e.
        //    m_compositionNode == 0 && !text.isEmpty().
        //    Sending a compositionupdate event at this time ensures that at least one
        //    compositionupdate event is dispatched.
        // 2. Updating the existing composition node.
        //    Send a compositionupdate event when this function updates the existing composition
        //    node, i.e. m_compositionNode != 0 && !text.isEmpty().
        // 3. Canceling the ongoing composition.
        //    Send a compositionend event when function deletes the existing composition node, i.e.
        //    m_compositionNode != 0 && test.isEmpty().
        if (!m_compositionNode) {
            // We should send a compositionstart event only when the given text is not empty because this
            // function doesn't create a composition node when the text is empty.
            if (!text.isEmpty()) {
                // When an inline predicition is being offered, there will be text and a non-zero amount of highlights.
                m_isHandlingAcceptedCandidate = !highlights.isEmpty();

                target->dispatchEvent(CompositionEvent::create(eventNames().compositionstartEvent, document->windowProxy(), originalText));
                event = CompositionEvent::create(eventNames().compositionupdateEvent, document->windowProxy(), text);
            }
        } else if (!text.isEmpty())
            event = CompositionEvent::create(eventNames().compositionupdateEvent, document->windowProxy(), text);

        if (event)
            target->dispatchEvent(*event);
    }

    // If text is empty, then delete the old composition here.  If text is non-empty, InsertTextCommand::input
    // will delete the old composition with an optimized replace operation.
    if (text.isEmpty()) {
        // The absence of text implies that there are currently no inline predicitions being offered.
        m_isHandlingAcceptedCandidate = false;

        TypingCommand::deleteSelection(document.copyRef(), TypingCommand::Option::PreventSpellChecking, TypingCommand::TextCompositionType::Pending);
        if (target)
            target->dispatchEvent(CompositionEvent::create(eventNames().compositionendEvent, document->windowProxy(), text));
    }

    RefPtr previousCompositionNode = m_compositionNode;
    m_compositionNode = nullptr;
    m_customCompositionUnderlines.clear();
    m_customCompositionHighlights.clear();
    m_customCompositionAnnotations.clear();

    if (!text.isEmpty()) {
        TypingCommand::insertText(document.copyRef(), text, event.get(), OptionSet { TypingCommand::Option::SelectInsertedText, TypingCommand::Option::PreventSpellChecking }, TypingCommand::TextCompositionType::Pending);

        // Find out what node has the composition now.
        Position base = document->selection().selection().base().downstream();
        Position extent = document->selection().selection().extent();
        RefPtr baseNode = base.deprecatedNode();
        unsigned baseOffset = base.deprecatedEditingOffset();
        RefPtr extentNode = extent.deprecatedNode();
        unsigned extentOffset = extent.deprecatedEditingOffset();

        if (is<Text>(baseNode) && baseNode == extentNode && baseOffset + text.length() == extentOffset) {
            m_compositionNode = static_pointer_cast<Text>(baseNode);
            m_compositionStart = baseOffset;
            m_compositionEnd = extentOffset;
            m_customCompositionUnderlines = underlines;
            for (auto& underline : m_customCompositionUnderlines) {
                underline.startOffset += baseOffset;
                underline.endOffset += baseOffset;
            }
            m_customCompositionHighlights = highlights;
            for (auto& highlight : m_customCompositionHighlights) {
                highlight.startOffset += baseOffset;
                highlight.endOffset += baseOffset;
            }

            m_customCompositionAnnotations = annotations;
            for (auto it = m_customCompositionAnnotations.begin(); it != m_customCompositionAnnotations.end(); ++it) {
                for (auto& range : it->value)
                    range.location += baseOffset;
            }

            if (auto renderer = baseNode->renderer())
                renderer->repaint();

            unsigned start = std::min(baseOffset + selectionStart, extentOffset);
            unsigned end = std::min(std::max(start, baseOffset + selectionEnd), extentOffset);
            auto range = SimpleRange { { *baseNode, start }, { *baseNode, end } };
            document->selection().setSelectedRange(range, Affinity::Downstream, FrameSelection::ShouldCloseTyping::No);
        }
    }

    if (CheckedPtr cache = document->existingAXObjectCache()) {
        if (previousCompositionNode && previousCompositionNode != m_compositionNode) {
            auto state = m_compositionNode ? CompositionState::InProgress : CompositionState::Ended;
            cache->onTextCompositionChange(*previousCompositionNode, state, true, text, m_compositionStart, m_isHandlingAcceptedCandidate);
        }
        if (RefPtr compositionNode = this->compositionNode()) {
            auto state = previousCompositionNode ? CompositionState::InProgress : CompositionState::Started;
            cache->onTextCompositionChange(*compositionNode, state, true, text, m_compositionStart, m_isHandlingAcceptedCandidate);
        }
    }

#if PLATFORM(IOS_FAMILY)        
    client()->stopDelayingAndCoalescingContentChangeNotifications();
#endif
}

void Editor::ignoreSpelling()
{
    if (!client())
        return;

    if (auto selectedRange = document().selection().selection().toNormalizedRange())
        removeMarkers(*selectedRange, DocumentMarkerType::Spelling);

    String text = selectedText();
    ASSERT(text.length());
    textChecker()->ignoreWordInSpellDocument(text);
}

void Editor::learnSpelling()
{
    if (!client())
        return;
        
    // FIXME: On macOS, when use "learn" button on "Spelling and Grammar" panel, we don't call this function. It should remove misspelling markers around the learned word, see <rdar://problem/5396072>.

    if (auto selectedRange = document().selection().selection().toNormalizedRange())
        removeMarkers(*selectedRange, DocumentMarkerType::Spelling);

    String text = selectedText();
    ASSERT(text.length());
    textChecker()->learnWord(text);
}

#if !PLATFORM(IOS_FAMILY)

void Editor::advanceToNextMisspelling(bool startBeforeSelection)
{
    Ref document = this->document();

    // The basic approach is to search in two phases: from the selection end to the end of the document, and
    // then we wrap and search from the document start to (approximately) where we started.

    // Start at the end of the selection, search to edge of document.
    // Starting at the selection end makes repeated "check spelling" commands work.
    VisibleSelection selection(document->selection().selection());
    auto spellingSearchRange = makeRangeSelectingNodeContents(document);

    bool startedWithSelection = false;
    if (selection.start().deprecatedNode()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            // Match AppKit's rule: Start 1 character before the selection.
            auto start = selection.visibleStart();
            auto beforeStart = start.previous();
            if (beforeStart.isNotNull())
                start = beforeStart;
            spellingSearchRange.start = *makeBoundaryPoint(start);
        } else
            spellingSearchRange.start = *makeBoundaryPoint(selection.visibleEnd());
    }

    auto position = makeDeprecatedLegacyPosition(spellingSearchRange.start);
    if (!isEditablePosition(position)) {
        // This shouldn't happen in very often because the Spelling menu items aren't enabled unless the
        // selection is editable.
        // This can happen in Mail for a mix of non-editable and editable content (like Stationary), 
        // when spell checking the whole document before sending the message.
        // In that case the document might not be editable, but there are editable pockets that need to be spell checked.

        position = VisiblePosition(firstEditablePositionAfterPositionInRoot(position, document->documentElement())).deepEquivalent();
        if (position.isNull())
            return;
        
        if (auto point = makeBoundaryPoint(position.parentAnchoredEquivalent()))
            spellingSearchRange.start = *point;
        startedWithSelection = false; // won't need to wrap
    }

    // topNode defines the whole range we want to operate on 
    RefPtr topNode { highestEditableRoot(position) };
    if (topNode)
        spellingSearchRange.end = makeBoundaryPointAfterNodeContents(*topNode);

    // If spellingSearchRange starts in the middle of a word, advance to the next word so we start checking
    // at a word boundary. Going back by one char and then forward by a word does the trick.
    if (startedWithSelection) {
        auto oneBeforeStart = VisiblePosition(makeContainerOffsetPosition(spellingSearchRange.start)).previous();
        if (oneBeforeStart.isNotNull())
            spellingSearchRange.start = *makeBoundaryPoint(endOfWord(oneBeforeStart));
        // else we were already at the start of the editable node
    }

    if (spellingSearchRange.collapsed())
        return; // nothing to search in

    // Get the spell checker if it is available
    if (!client())
        return;

    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    auto searchEndAfterWrap = spellingSearchRange.end;
    
    TextCheckingHelper::MisspelledWord misspelledWord;
    TextCheckingHelper::UngrammaticalPhrase ungrammaticalPhrase;
    auto grammarSearchRange = spellingSearchRange;

    if (unifiedTextCheckerEnabled()) {
        auto foundItem = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelledWordOrUngrammaticalPhrase(isGrammarCheckingEnabled());
        if (auto* word = std::get_if<TextCheckingHelper::MisspelledWord>(&foundItem))
            misspelledWord = WTFMove(*word);
        else
            ungrammaticalPhrase = std::get<TextCheckingHelper::UngrammaticalPhrase>(WTFMove(foundItem));
    } else {
        misspelledWord = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelledWord();

        if (!misspelledWord.word.isEmpty()) {
            // Stop looking at start of next misspelled word
            CharacterIterator characterIterator(grammarSearchRange);
            characterIterator.advance(misspelledWord.offset);
            grammarSearchRange.end = characterIterator.range().start;
        }

        if (isGrammarCheckingEnabled())
            ungrammaticalPhrase = TextCheckingHelper(*client(), grammarSearchRange).findFirstUngrammaticalPhrase();
    }

    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && misspelledWord.word.isEmpty() && ungrammaticalPhrase.phrase.isEmpty()) {
        if (topNode)
            spellingSearchRange.start = makeBoundaryPointBeforeNodeContents(*topNode);
        spellingSearchRange.end = searchEndAfterWrap;
        
        if (unifiedTextCheckerEnabled()) {
            auto foundItem = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelledWordOrUngrammaticalPhrase(isGrammarCheckingEnabled());
            if (auto* word = std::get_if<TextCheckingHelper::MisspelledWord>(&foundItem))
                misspelledWord = WTFMove(*word);
            else
                ungrammaticalPhrase = std::get<TextCheckingHelper::UngrammaticalPhrase>(WTFMove(foundItem));
        } else {
            misspelledWord = TextCheckingHelper(*client(), spellingSearchRange).findFirstMisspelledWord();

            grammarSearchRange = spellingSearchRange;
            if (!misspelledWord.word.isEmpty()) {
                // Stop looking at start of next misspelled word
                CharacterIterator characterIterator(grammarSearchRange);
                characterIterator.advance(misspelledWord.offset);
                grammarSearchRange.end = characterIterator.range().start;
            }

            if (isGrammarCheckingEnabled())
                ungrammaticalPhrase = TextCheckingHelper(*client(), grammarSearchRange).findFirstUngrammaticalPhrase();
        }
    }
    
    if (!ungrammaticalPhrase.phrase.isEmpty()) {
        // We found bad grammar. Since we only searched for bad grammar up to the first misspelled word, the bad grammar
        // takes precedence and we ignore any potential misspelled word. Select the grammar detail, update the spelling
        // panel, and store a marker so we draw the green squiggle later.
        
        ASSERT(ungrammaticalPhrase.phrase.length() > 0);
        ASSERT(ungrammaticalPhrase.detail.range.length > 0);
        
        // FIXME 4859190: This gets confused with doubled punctuation at the end of a paragraph.
        auto badGrammarRange = resolveCharacterRange(grammarSearchRange, { ungrammaticalPhrase.offset + ungrammaticalPhrase.detail.range.location, ungrammaticalPhrase.detail.range.length });
        document->selection().setSelection(VisibleSelection(badGrammarRange));
        document->selection().revealSelection();
        
        client()->updateSpellingUIWithGrammarString(ungrammaticalPhrase.phrase, ungrammaticalPhrase.detail);
        addMarker(badGrammarRange, DocumentMarkerType::Grammar, ungrammaticalPhrase.detail.userDescription);
    } else if (!misspelledWord.word.isEmpty()) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.
        
        auto misspellingRange = resolveCharacterRange(spellingSearchRange, { misspelledWord.offset, misspelledWord.word.length() });
        document->selection().setSelection(VisibleSelection(misspellingRange));
        document->selection().revealSelection();
        
        client()->updateSpellingUIWithMisspelledWord(misspelledWord.word);
        addMarker(misspellingRange, DocumentMarkerType::Spelling);
    }
}

#endif // !PLATFORM(IOS_FAMILY)

String Editor::misspelledWordAtCaretOrRange(Node* clickedNode) const
{
    if (!isContinuousSpellCheckingEnabled() || !clickedNode || !isSpellCheckingEnabledFor(clickedNode))
        return String();

    VisibleSelection selection = document().selection().selection();
    if (!selection.isContentEditable() || selection.isNone())
        return String();

    VisibleSelection wordSelection(selection.base());
    wordSelection.expandUsingGranularity(TextGranularity::WordGranularity);
    auto wordRange = wordSelection.toNormalizedRange();
    if (!wordRange)
        return String();

    // In compliance with GTK+ applications, additionally allow to provide suggestions when the current
    // selection exactly match the word selection.
    if (selection.isRange() && *wordRange != selection.toNormalizedRange())
        return String();

    String word = plainText(*wordRange);
    if (word.isEmpty() || !client())
        return String();

    int wordLength = word.length();
    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker()->checkSpellingOfString(word, &misspellingLocation, &misspellingLength);

    return misspellingLength == wordLength ? word : String();
}

String Editor::misspelledSelectionString() const
{
    String selectedString = selectedText();
    int length = selectedString.length();
    if (!length || !client())
        return String();

    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker()->checkSpellingOfString(selectedString, &misspellingLocation, &misspellingLength);
    
    // The selection only counts as misspelled if the selected text is exactly one misspelled word
    if (misspellingLength != length)
        return String();
    
    // Update the spelling panel to be displaying this error (whether or not the spelling panel is on screen).
    // This is necessary to make a subsequent call to [NSSpellChecker ignoreWord:inSpellDocumentWithTag:] work
    // correctly; that call behaves differently based on whether the spelling panel is displaying a misspelling
    // or a grammar error.
    client()->updateSpellingUIWithMisspelledWord(selectedString);
    
    return selectedString;
}

Vector<String> Editor::guessesForMisspelledWord(const String& word) const
{
    ASSERT(word.length());

    Vector<String> guesses;
    if (client())
        textChecker()->getGuessesForWord(word, String(), document().selection().selection(), guesses);
    return guesses;
}

TextCheckingGuesses Editor::guessesForMisspelledOrUngrammatical()
{
    if (unifiedTextCheckerEnabled()) {
        std::optional<SimpleRange> range;
        auto selection = document().selection().selection();
        if (selection.isCaret() && behavior().shouldAllowSpellingSuggestionsWithoutSelection()) {
            auto wordSelection = VisibleSelection(selection.base());
            wordSelection.expandUsingGranularity(TextGranularity::WordGranularity);
            range = wordSelection.toNormalizedRange();
        } else
            range = selection.toNormalizedRange();
        if (!range || !client())
            return { };
        return TextCheckingHelper(*client(), *range).guessesForMisspelledWordOrUngrammaticalPhrase(isGrammarCheckingEnabled());
    }

    auto misspelledWord = behavior().shouldAllowSpellingSuggestionsWithoutSelection() ? misspelledWordAtCaretOrRange(document().focusedElement()) : misspelledSelectionString();
    if (misspelledWord.isEmpty())
        return { };
    return { guessesForMisspelledWord(misspelledWord), true, false };
}

void Editor::showSpellingGuessPanel()
{
    if (!client()) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    if (client()->spellingUIIsShowing()) {
        client()->showSpellingUI(false);
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    advanceToNextMisspelling(true);
#endif
    client()->showSpellingUI(true);
}

bool Editor::spellingPanelIsShowing()
{
    if (!client())
        return false;
    return client()->spellingUIIsShowing();
}

void Editor::clearMisspellingsAndBadGrammar(const VisibleSelection& movingSelection)
{
    if (auto selectedRange = movingSelection.toNormalizedRange())
        removeMarkers(*selectedRange, { DocumentMarkerType::Spelling, DocumentMarkerType::Grammar });
}

void Editor::markMisspellingsAndBadGrammar(const VisibleSelection &movingSelection)
{
    markMisspellingsAndBadGrammar(movingSelection, isContinuousSpellCheckingEnabled() && isGrammarCheckingEnabled(), movingSelection);
}

void Editor::markMisspellingsAfterTypingToWord(const VisiblePosition& wordStart, const VisibleSelection& selectionAfterTyping, AllowTextReplacement allowTextReplacement)
{
    Ref document = this->document();

    if (platformOrClientDrivenTextCheckerEnabled())
        return;

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(selectionAfterTyping);
    UNUSED_PARAM(allowTextReplacement);
    OptionSet<TextCheckingType> textCheckingOptions;
    if (isContinuousSpellCheckingEnabled())
        textCheckingOptions.add(TextCheckingType::Spelling);
    if (!textCheckingOptions.contains(TextCheckingType::Spelling))
        return;

    auto adjacentWords = VisibleSelection(startOfWord(wordStart, WordSide::LeftWordIfOnBoundary), endOfWord(wordStart, WordSide::RightWordIfOnBoundary));
    auto adjacentWordRange = adjacentWords.toNormalizedRange();

#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
    if (isGrammarCheckingEnabled()) {
        textCheckingOptions.add(TextCheckingType::Grammar);
        textCheckingOptions.add(TextCheckingType::Correction);
        auto sentenceStart = startOfSentence(wordStart);
        auto sentenceEnd = endOfSentence(wordStart);
        VisibleSelection fullSentence(sentenceStart, sentenceEnd);
        auto fullSentenceRange = fullSentence.toNormalizedRange();
        if (!fullSentenceRange)
            return;

        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWordRange, adjacentWordRange, fullSentenceRange);
    } else
#endif
    {
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWordRange, adjacentWordRange, adjacentWordRange);
    }
#else
#if !USE(AUTOMATIC_TEXT_REPLACEMENT)
    UNUSED_PARAM(allowTextReplacement);
#endif

    if (unifiedTextCheckerEnabled()) {
        m_alternativeTextController->applyPendingCorrection(selectionAfterTyping);

        OptionSet<TextCheckingType> textCheckingOptions;

        if (isContinuousSpellCheckingEnabled())
            textCheckingOptions.add(TextCheckingType::Spelling);

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
        if (allowTextReplacement == AllowTextReplacement::Yes
            && (isAutomaticQuoteSubstitutionEnabled()
                || isAutomaticLinkDetectionEnabled()
                || isAutomaticDashSubstitutionEnabled()
                || isAutomaticTextReplacementEnabled()
                || (textCheckingOptions.contains(TextCheckingType::Spelling) && isAutomaticSpellingCorrectionEnabled())))
            textCheckingOptions.add(TextCheckingType::Replacement);
#endif
        if (!textCheckingOptions.contains(TextCheckingType::Spelling) && !textCheckingOptions.contains(TextCheckingType::Replacement))
            return;

        if (isGrammarCheckingEnabled())
            textCheckingOptions.add(TextCheckingType::Grammar);

        auto sentenceStart = startOfSentence(wordStart);
        auto sentenceEnd = endOfSentence(wordStart);
        VisibleSelection fullSentence(sentenceStart, sentenceEnd);
        auto fullSentenceRange = fullSentence.toNormalizedRange();
        if (!fullSentenceRange)
            return;

        auto spellCheckingStart = wordStart;
        auto spellCheckingEnd = wordStart;

        // FIXME: The following logic doesn't handle adding spelling markers due to retro sentence corrections when an
        // incorrectly spelled range is separated from the start of the current word by a text node inside an element
        // with spellcheck disabled. To fix this, we need to refactor markAllMisspellingsAndBadGrammarInRanges so that
        // it can handle a list of spelling ranges, alongside the grammar range.
        while (sentenceStart < spellCheckingStart) {
            auto previousPosition = spellCheckingStart.previous(CannotCrossEditingBoundary);
            if (previousPosition.isNull() || previousPosition == spellCheckingStart)
                break;

            RefPtr container = previousPosition.deepEquivalent().downstream().containerNode();
            RefPtr containerElement = dynamicDowncast<Element>(container);
            if (!containerElement)
                containerElement = container->parentElement();
            if (containerElement && !containerElement->isSpellCheckingEnabled())
                break;

            spellCheckingStart = previousPosition;
        }

        while (spellCheckingEnd < sentenceEnd) {
            auto nextPosition = spellCheckingEnd.next(CannotCrossEditingBoundary);
            if (nextPosition.isNull() || nextPosition == spellCheckingEnd)
                break;

            if (RefPtr containerElement = nextPosition.deepEquivalent().upstream().containerOrParentElement()) {
                if (!containerElement->isSpellCheckingEnabled())
                    break;
            }

            spellCheckingEnd = nextPosition;
        }

        auto spellCheckingRange = VisibleSelection(spellCheckingStart, spellCheckingEnd).toNormalizedRange();
        if (!spellCheckingRange)
            return;

        auto adjacentWordRange = intersection(VisibleSelection(startOfWord(wordStart, WordSide::LeftWordIfOnBoundary), endOfWord(wordStart, WordSide::RightWordIfOnBoundary)).toNormalizedRange(), fullSentenceRange);
        if (!adjacentWordRange)
            return;

        // The spelling and grammar markers in these ranges are recomputed. This is because typing a word may
        // cause any other part of the current sentence to lose or gain spelling correction markers, due to
        // sentence retro correction. As such, we expand the spell checking range to encompass as much of the
        // full sentence as we can, respecting boundaries where spellchecking is disabled.
        removeMarkers(*fullSentenceRange, DocumentMarkerType::Grammar);
        removeMarkers(*spellCheckingRange, DocumentMarkerType::Spelling);
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, spellCheckingRange, adjacentWordRange, fullSentenceRange);
        return;
    }

    if (!isContinuousSpellCheckingEnabled())
        return;

    // Check spelling of one word
    auto misspellingRange = markMisspellings(VisibleSelection(startOfWord(wordStart, WordSide::LeftWordIfOnBoundary), endOfWord(wordStart, WordSide::RightWordIfOnBoundary)));

    // Autocorrect the misspelled word.
    if (!misspellingRange)
        return;

    if (!isGrammarCheckingEnabled())
        return;
    
    // Check grammar of entire sentence
    markBadGrammar(VisibleSelection(startOfSentence(wordStart), endOfSentence(wordStart)));
#endif
}
    
std::optional<SimpleRange> Editor::markMisspellingsOrBadGrammar(const VisibleSelection& selection, bool checkSpelling)
{
#if !PLATFORM(IOS_FAMILY)
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.
    
    // This function is used only for as-you-type checking, so if that's off we do nothing. Note that
    // grammar checking can only be on if spell checking is also on.
    if (!isContinuousSpellCheckingEnabled())
        return std::nullopt;
    
    auto searchRange = selection.toNormalizedRange();
    if (!searchRange)
        return std::nullopt;
    
    // If we're not in an editable node, bail.
    Ref editableNode = searchRange->startContainer();
    if (!editableNode->hasEditableStyle())
        return std::nullopt;

    if (!isSpellCheckingEnabledFor(editableNode.ptr()))
        return std::nullopt;

    // Get the spell checker if it is available
    if (!client())
        return std::nullopt;
    
    TextCheckingHelper checker(*client(), *searchRange);
    if (checkSpelling)
        return checker.markAllMisspelledWords();
    if (isGrammarCheckingEnabled())
        checker.markAllUngrammaticalPhrases();
    return std::nullopt;
#else
    UNUSED_PARAM(selection);
    UNUSED_PARAM(checkSpelling);
    return std::nullopt;
#endif
}

bool Editor::isSpellCheckingEnabledFor(Node* node) const
{
    if (!node)
        return false;
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        element = node->parentElement();
    if (!element)
        return false;
    if (element->isInUserAgentShadowTree()) {
        if (RefPtr textControl = enclosingTextFormControl(firstPositionInOrBeforeNode(element.get())))
            return textControl->isSpellCheckingEnabled();
    }
    return element->isSpellCheckingEnabled();
}

bool Editor::isSpellCheckingEnabledInFocusedNode() const
{
    return isSpellCheckingEnabledFor(document().selection().selection().start().deprecatedNode());
}

std::optional<SimpleRange> Editor::markMisspellings(const VisibleSelection& selection)
{
    return markMisspellingsOrBadGrammar(selection, true);
}
    
void Editor::markBadGrammar(const VisibleSelection& selection)
{
    markMisspellingsOrBadGrammar(selection, false);
}

void Editor::markAllMisspellingsAndBadGrammarInRanges(OptionSet<TextCheckingType> textCheckingOptions, const std::optional<SimpleRange>& spellingRange, const std::optional<SimpleRange>& automaticReplacementRange, const std::optional<SimpleRange>& grammarRange)
{
    if (platformOrClientDrivenTextCheckerEnabled())
        return;

    ASSERT(unifiedTextCheckerEnabled());

    // There shouldn't be pending autocorrection at this moment.
    ASSERT(!m_alternativeTextController->hasPendingCorrection());

    bool shouldMarkGrammar = textCheckingOptions.contains(TextCheckingType::Grammar);
    bool shouldShowCorrectionPanel = textCheckingOptions.contains(TextCheckingType::ShowCorrectionPanel);

    // This function is called with selections already expanded to word boundaries.
    if (!client() || !spellingRange || (shouldMarkGrammar && !grammarRange))
        return;

    // Do not mark spelling or grammer corrections when an inline prediction candidate is currently being offered.
    if (m_isHandlingAcceptedCandidate)
        return;

    // If we're not in an editable node, bail.
    Ref editableNode = spellingRange->startContainer();
    if (!editableNode->hasEditableStyle())
        return;

    if (!isSpellCheckingEnabledFor(editableNode.ptr()))
        return;

    auto& rangeToCheck = shouldMarkGrammar ? *grammarRange : *spellingRange;
    TextCheckingParagraph paragraphToCheck(rangeToCheck);
    if (paragraphToCheck.isEmpty())
        return;

    bool asynchronous = document().settings().asynchronousSpellCheckingEnabled() && !shouldShowCorrectionPanel;

    // In asynchronous mode, we intentionally check paragraph-wide sentence.
    auto resolvedOptions = resolveTextCheckingTypeMask(editableNode, textCheckingOptions);
    auto& paragraphRange = paragraphToCheck.paragraphRange();
    auto& checkingRange = asynchronous ? paragraphRange : rangeToCheck;
    auto& textReplacementRange = automaticReplacementRange ? *automaticReplacementRange : rangeToCheck;
    auto request = SpellCheckRequest::create(resolvedOptions, TextCheckingProcessType::TextCheckingProcessIncremental, checkingRange, textReplacementRange, paragraphRange);
    if (!request)
        return;

    if (asynchronous) {
        m_spellChecker->requestCheckingFor(request.releaseNonNull());
        return;
    }

    Vector<TextCheckingResult> results;
    checkTextOfParagraph(*textChecker(), paragraphToCheck.text(), resolvedOptions, results, document().selection().selection());
    markAndReplaceFor(request.releaseNonNull(), results);
}

static bool isAutomaticTextReplacementType(TextCheckingType type)
{
    switch (type) {
    case TextCheckingType::None:
    case TextCheckingType::Spelling:
    case TextCheckingType::Grammar:
        return false;
    case TextCheckingType::Link:
    case TextCheckingType::Quote:
    case TextCheckingType::Dash:
    case TextCheckingType::Replacement:
    case TextCheckingType::Correction:
    case TextCheckingType::ShowCorrectionPanel:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void Editor::replaceRangeForSpellChecking(const SimpleRange& rangeToReplace, const String& replacement)
{
    SpellingCorrectionCommand::create(rangeToReplace, replacement)->apply();
}

static void correctSpellcheckingPreservingTextCheckingParagraph(TextCheckingParagraph& paragraph, const SimpleRange& rangeToReplace, const String& replacement, CharacterRange resultCharacterRange)
{
    Ref scopeNode = downcast<ContainerNode>(paragraph.paragraphRange().startContainer().rootNode());
    auto paragraphCharacterRange = characterRange(makeBoundaryPointBeforeNodeContents(scopeNode), paragraph.paragraphRange());

    SpellingCorrectionCommand::create(rangeToReplace, replacement)->apply();

    // TextCheckingParagraph may be orphaned after SpellingCorrectionCommand mutated DOM.
    // See <rdar://10305315>, http://webkit.org/b/89526.

    paragraphCharacterRange.length += replacement.length() - resultCharacterRange.length;
    auto newParagraphRange = resolveCharacterRange(makeRangeSelectingNodeContents(scopeNode), paragraphCharacterRange);
    auto spellCheckingRange = resolveCharacterRange(newParagraphRange, { resultCharacterRange.location, replacement.length() });
    paragraph = TextCheckingParagraph(spellCheckingRange, spellCheckingRange, newParagraphRange);
}

void Editor::markAndReplaceFor(const SpellCheckRequest& request, const Vector<TextCheckingResult>& results)
{
    Ref document = this->document();

    auto textCheckingOptions = request.data().checkingTypes();
    TextCheckingParagraph paragraph(request.checkingRange(), request.automaticReplacementRange(), request.paragraphRange());

    const bool shouldPerformReplacement = textCheckingOptions.containsAny({ TextCheckingType::Quote, TextCheckingType::Dash, TextCheckingType::Replacement });
    const bool shouldMarkSpelling = textCheckingOptions.contains(TextCheckingType::Spelling);
    const bool shouldMarkGrammar = textCheckingOptions.contains(TextCheckingType::Grammar);
    const bool shouldMarkLink = textCheckingOptions.contains(TextCheckingType::Link);
    const bool shouldShowCorrectionPanel = textCheckingOptions.contains(TextCheckingType::ShowCorrectionPanel);
    const bool shouldCheckForCorrection = shouldShowCorrectionPanel || textCheckingOptions.contains(TextCheckingType::Correction);
#if !USE(AUTOCORRECTION_PANEL)
    ASSERT(!shouldShowCorrectionPanel);
#endif

    // Expand the range to encompass entire paragraphs, since text checking needs that much context.
    uint64_t selectionOffset = 0;
    bool useAmbiguousBoundaryOffset = false;
    bool selectionChanged = false;
    bool restoreSelectionAfterChange = false;

    if (shouldPerformReplacement || shouldMarkSpelling || shouldCheckForCorrection) {
        if (document->selection().isCaret()) {
            // Attempt to save the caret position so we can restore it later if needed
            Position caretPosition = document->selection().selection().end();
            selectionOffset = paragraph.offsetTo(caretPosition).releaseReturnValue();
            restoreSelectionAfterChange = true;
            if (selectionOffset > 0 && selectionOffset <= paragraph.text().length() && isAmbiguousBoundaryCharacter(paragraph.text()[selectionOffset - 1]))
                useAmbiguousBoundaryOffset = true;
        }
    }

    int offsetDueToReplacement = 0;
    Vector<CharacterRange> previousGrammarRanges;

    for (unsigned i = 0; i < results.size(); i++) {
        auto spellingRangeEndOffset = paragraph.checkingEnd() + offsetDueToReplacement;
        if (!results[i].type.hasExactlyOneBitSet()) {
            ASSERT_NOT_REACHED();
            continue;
        }
        TextCheckingType resultType = *results[i].type.toSingleValue();
        auto resultLocation = results[i].range.location + offsetDueToReplacement;
        auto resultLength = results[i].range.length;
        auto resultEndLocation = resultLocation + resultLength;
        auto automaticReplacementStartLocation = paragraph.automaticReplacementStart();
        auto automaticReplacementEndLocation = automaticReplacementStartLocation + paragraph.automaticReplacementLength() + offsetDueToReplacement;
        const String& replacement = results[i].replacement;
        bool resultEndsAtAmbiguousBoundary = useAmbiguousBoundaryOffset && selectionOffset - 1 <= resultEndLocation;

        bool resultRangeIsAcceptableForReplacement = automaticReplacementStartLocation <= resultEndLocation && resultEndLocation <= automaticReplacementEndLocation;
        // In this case the result range just has to touch the automatic replacement range, so we can handle replacing non-word text such as punctuation.
#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
        if (!resultRangeIsAcceptableForReplacement && shouldCheckForCorrection && resultType == TextCheckingType::Correction)
            resultRangeIsAcceptableForReplacement = results[i].details.size() > 0;
        if (!resultRangeIsAcceptableForReplacement && shouldMarkGrammar && shouldCheckForCorrection && resultType == TextCheckingType::Correction) {
            resultRangeIsAcceptableForReplacement = previousGrammarRanges.containsIf([&](auto& range) {
                return range.location == resultLocation && range.length == resultLength;
            });
        }
#endif

        // Only mark misspelling if:
        // 1. Current text checking isn't done for autocorrection, in which case shouldMarkSpelling is false.
        // 2. Result falls within spellingRange.
        // 3. The word in question doesn't end at an ambiguous boundary. For instance, we would not mark
        //    "wouldn'" as misspelled right after apostrophe is typed.
        if (shouldMarkSpelling && !shouldShowCorrectionPanel && resultType == TextCheckingType::Spelling
            && resultLocation >= paragraph.checkingStart() && resultEndLocation <= spellingRangeEndOffset && !resultEndsAtAmbiguousBoundary) {
            ASSERT(resultLength > 0);
            auto misspellingRange = paragraph.subrange({ resultLocation, resultLength });
            if (!m_alternativeTextController->isSpellingMarkerAllowed(misspellingRange))
                continue;
            addMarker(misspellingRange, DocumentMarkerType::Spelling, replacement);
        } else if (shouldMarkGrammar && resultType == TextCheckingType::Grammar && paragraph.checkingRangeCovers({ resultLocation, resultLength })) {
            ASSERT(resultLength > 0);
            for (auto& detail : results[i].details) {
                ASSERT(detail.range.length > 0);
                if (paragraph.checkingRangeCovers({ resultLocation + detail.range.location, detail.range.length })) {
                    auto badGrammarRange = paragraph.subrange({ resultLocation + detail.range.location, detail.range.length });
                    addMarker(badGrammarRange, DocumentMarkerType::Grammar, detail.userDescription);
                    previousGrammarRanges.append(CharacterRange(resultLocation + detail.range.location, detail.range.length));
                }
            }
        } else if (resultRangeIsAcceptableForReplacement && isAutomaticTextReplacementType(resultType)) {
            ASSERT(resultLength > 0);

            if (shouldShowCorrectionPanel && (resultEndLocation < automaticReplacementEndLocation
                || (resultType != TextCheckingType::Replacement && resultType != TextCheckingType::Correction)))
                continue;

            // Apply replacement if:
            // 1. The replacement length is non-zero.
            // 2. The result doesn't end at an ambiguous boundary.
            //    (FIXME: this is required until 6853027 is fixed and text checking can do this for us
            bool doReplacement = replacement.length() > 0 && !resultEndsAtAmbiguousBoundary;
            auto rangeToReplace = paragraph.subrange({ resultLocation, resultLength });

            // Adding links should be done only immediately after they are typed.
            if (resultType == TextCheckingType::Link && selectionOffset != resultEndLocation + 1)
                continue;

            if (!(shouldPerformReplacement || shouldCheckForCorrection || shouldMarkLink) || !doReplacement)
                continue;

            String replacedString = plainText(rangeToReplace);
            bool existingMarkersPermitReplacement = m_alternativeTextController->processMarkersOnTextToBeReplacedByResult(results[i], rangeToReplace, replacedString);
#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
            if (!existingMarkersPermitReplacement && shouldCheckForCorrection && resultType == TextCheckingType::Correction)
                existingMarkersPermitReplacement = results[i].details.size() > 0;
#endif
            if (!existingMarkersPermitReplacement)
                continue;

            if (shouldShowCorrectionPanel) {
                if (resultEndLocation == automaticReplacementEndLocation) {
                    // We only show the correction panel on the last word.
                    m_alternativeTextController->show(rangeToReplace, replacement);
                    break;
                }
                // If this function is called for showing correction panel, we ignore other correction or replacement.
                continue;
            }

            VisibleSelection selectionToReplace(rangeToReplace);
            if (selectionToReplace != document->selection().selection()) {
                if (!document->selection().shouldChangeSelection(selectionToReplace))
                    continue;
            }

            if (resultType == TextCheckingType::Link) {
                document->selection().setSelection(selectionToReplace);
                selectionChanged = true;
                restoreSelectionAfterChange = false;
                if (canEditRichly())
                    CreateLinkCommand::create(document.copyRef(), replacement)->apply();
            } else if (canEdit() && shouldInsertText(replacement, rangeToReplace, EditorInsertAction::Typed)) {
                correctSpellcheckingPreservingTextCheckingParagraph(paragraph, rangeToReplace, replacement, { resultLocation, resultLength });

                if (AXObjectCache* cache = document->existingAXObjectCache()) {
                    if (RefPtr root = document->selection().selection().rootEditableElement())
                        cache->postNotification(root.get(), AXNotification::AutocorrectionOccured);
                }

                // Skip all other results for the replaced text.
                while (i + 1 < results.size() && results[i + 1].range.location + offsetDueToReplacement <= resultLocation)
                    i++;

                selectionChanged = true;
                offsetDueToReplacement += replacement.length() - resultLength;
                if (resultLocation < selectionOffset)
                    selectionOffset += replacement.length() - resultLength;

                if (resultType == TextCheckingType::Correction) {
                    auto replacementRange = paragraph.subrange({ resultLocation, replacement.length() });
                    m_alternativeTextController->recordAutocorrectionResponse(AutocorrectionResponse::Accepted, replacedString, replacementRange);

                    // Add a marker so that corrections can easily be undone and won't be re-corrected.
                    m_alternativeTextController->markCorrection(replacementRange, replacedString);
                }
            }
        }
    }

    if (selectionChanged) {
        TextCheckingParagraph extendedParagraph(WTFMove(paragraph));
        // Restore the caret position if we have made any replacements
        extendedParagraph.expandRangeToNextEnd();
        if (restoreSelectionAfterChange && selectionOffset <= extendedParagraph.rangeLength()) {
            auto selectionRange = extendedParagraph.subrange({ 0, selectionOffset });
            document->selection().moveTo(makeContainerOffsetPosition(selectionRange.end), Affinity::Downstream);
        } else {
            // If this fails for any reason, the fallback is to go one position beyond the last replacement
            document->selection().moveTo(document->selection().selection().end());
            document->selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::CharacterGranularity);
        }
    }
}

void Editor::changeBackToReplacedString(const String& replacedString)
{
#if !PLATFORM(IOS_FAMILY)
    ASSERT(unifiedTextCheckerEnabled());

    if (replacedString.isEmpty())
        return;

    auto selection = selectedRange();
    if (!selection || !shouldInsertText(replacedString, selection, EditorInsertAction::Pasted))
        return;
    
    m_alternativeTextController->recordAutocorrectionResponse(AutocorrectionResponse::Reverted, replacedString, *selection);
    TextCheckingParagraph paragraph(*selection);
    replaceSelectionWithText(replacedString, SelectReplacement::No, SmartReplace::No, EditAction::Insert);
    auto changedRange = paragraph.subrange(CharacterRange(paragraph.checkingStart(), replacedString.length()));
    addMarker(changedRange, DocumentMarkerType::Replacement, String());
    m_alternativeTextController->markReversed(changedRange);
#else
    ASSERT_NOT_REACHED();
    UNUSED_PARAM(replacedString);
#endif // !PLATFORM(IOS_FAMILY)
}


void Editor::markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection)
{
    if (platformOrClientDrivenTextCheckerEnabled())
        return;

    if (unifiedTextCheckerEnabled()) {
        if (!isContinuousSpellCheckingEnabled())
            return;

        // markMisspellingsAndBadGrammar() is triggered by selection change, in which case we check spelling and grammar, but don't autocorrect misspellings.
        OptionSet<TextCheckingType> textCheckingOptions { TextCheckingType::Spelling };
        if (markGrammar && isGrammarCheckingEnabled())
            textCheckingOptions.add(TextCheckingType::Grammar);
        auto spellCheckingRange = spellingSelection.toNormalizedRange();
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, spellCheckingRange, spellCheckingRange, grammarSelection.toNormalizedRange());
        return;
    }

    markMisspellings(spellingSelection);
    if (markGrammar)
        markBadGrammar(grammarSelection);
}

void Editor::unappliedSpellCorrection(const VisibleSelection& selectionOfCorrected, const String& corrected, const String& correction)
{
    m_alternativeTextController->respondToUnappliedSpellCorrection(selectionOfCorrected, corrected, correction);
}

void Editor::updateMarkersForWordsAffectedByEditing(bool doNotRemoveIfSelectionAtWordBoundary)
{
    Ref document = this->document();
    CheckedPtr markers = document->markersIfExists();
    if (!markers || !markers->hasMarkers())
        return;

    if (!m_alternativeTextController->shouldRemoveMarkersUponEditing() && (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingType::Spelling)))
        return;

    // We want to remove the markers from a word if an editing command will change the word. This can happen in one of
    // several scenarios:
    // 1. Insert in the middle of a word.
    // 2. Appending non whitespace at the beginning of word.
    // 3. Appending non whitespace at the end of word.
    // Note that, appending only whitespaces at the beginning or end of word won't change the word, so we don't need to
    // remove the markers on that word.
    // Of course, if current selection is a range, we potentially will edit two words that fall on the boundaries of
    // selection, and remove words between the selection boundaries.
    //
    VisiblePosition startOfSelection = document->selection().selection().start();
    VisiblePosition endOfSelection = document->selection().selection().end();
    if (startOfSelection.isNull())
        return;
    // First word is the word that ends after or on the start of selection.
    VisiblePosition startOfFirstWord = startOfWord(startOfSelection, WordSide::LeftWordIfOnBoundary);
    VisiblePosition endOfFirstWord = endOfWord(startOfSelection, WordSide::LeftWordIfOnBoundary);
    // Last word is the word that begins before or on the end of selection
    VisiblePosition startOfLastWord = startOfWord(endOfSelection, WordSide::RightWordIfOnBoundary);
    VisiblePosition endOfLastWord = endOfWord(endOfSelection, WordSide::RightWordIfOnBoundary);

    if (startOfFirstWord.isNull()) {
        startOfFirstWord = startOfWord(startOfSelection, WordSide::RightWordIfOnBoundary);
        endOfFirstWord = endOfWord(startOfSelection, WordSide::RightWordIfOnBoundary);
    }
    
    if (endOfLastWord.isNull()) {
        startOfLastWord = startOfWord(endOfSelection, WordSide::LeftWordIfOnBoundary);
        endOfLastWord = endOfWord(endOfSelection, WordSide::LeftWordIfOnBoundary);
    }

    auto originalEndOfFirstWord = endOfFirstWord;
    auto originalStartOfLastWord = startOfLastWord;

    // If doNotRemoveIfSelectionAtWordBoundary is true, and first word ends at the start of selection,
    // we choose next word as the first word.
    if (doNotRemoveIfSelectionAtWordBoundary && endOfFirstWord == startOfSelection) {
        startOfFirstWord = nextWordPosition(startOfFirstWord);
        endOfFirstWord = endOfWord(startOfFirstWord, WordSide::RightWordIfOnBoundary);
        if (startOfFirstWord == originalStartOfLastWord)
            return;
    }

    // If doNotRemoveIfSelectionAtWordBoundary is true, and last word begins at the end of selection,
    // we choose previous word as the last word.
    if (doNotRemoveIfSelectionAtWordBoundary && startOfLastWord == endOfSelection) {
        startOfLastWord = previousWordPosition(startOfLastWord);
        endOfLastWord = endOfWord(startOfLastWord, WordSide::RightWordIfOnBoundary);
        if (endOfLastWord == originalEndOfFirstWord)
            return;
    }

    if (startOfFirstWord.isNull() || endOfFirstWord.isNull() || startOfLastWord.isNull() || endOfLastWord.isNull())
        return;

    // Now we remove markers on everything between startOfFirstWord and endOfLastWord.
    // However, if an autocorrection change a single word to multiple words, we want to remove correction mark from all the
    // resulted words even we only edit one of them. For example, assuming autocorrection changes "avantgarde" to "avant
    // garde", we will have CorrectionIndicator marker on both words and on the whitespace between them. If we then edit garde,
    // we would like to remove the marker from word "avant" and whitespace as well. So we need to get the continous range of
    // of marker that contains the word in question, and remove marker on that whole range.
    auto wordRange = *makeSimpleRange(startOfFirstWord, endOfLastWord);

    for (auto& marker : markers->markersInRange(wordRange, DocumentMarkerType::DictationAlternatives))
        m_alternativeTextController->removeDictationAlternativesForMarker(*marker);

    OptionSet<DocumentMarkerType> markerTypesToRemove {
        DocumentMarkerType::CorrectionIndicator,
        DocumentMarkerType::SpellCheckingExemption,
        DocumentMarkerType::Spelling,
#if !PLATFORM(IOS_FAMILY)
        DocumentMarkerType::Grammar,
#endif
    };

    if (CheckedPtr client = this->client(); client && client->shouldRemoveDictationAlternativesAfterEditing())
        markerTypesToRemove.add(DocumentMarkerType::DictationAlternatives);

    adjustMarkerTypesToRemoveForWordsAffectedByEditing(markerTypesToRemove);

    removeMarkers(wordRange, markerTypesToRemove, RemovePartiallyOverlappingMarker::Yes);
    markers->clearDescriptionOnMarkersIntersectingRange(wordRange, DocumentMarkerType::Replacement);
}

void Editor::deletedAutocorrectionAtPosition(const Position& position, const String& originalString)
{
    m_alternativeTextController->deletedAutocorrectionAtPosition(position, originalString);
}

std::optional<SimpleRange> Editor::rangeForPoint(const IntPoint& windowPoint)
{
    RefPtr document = this->document().frame()->documentAtPoint(windowPoint);
    if (!document)
        return std::nullopt;
    RefPtr frame = document->frame();
    if (!frame)
        return std::nullopt;
    RefPtr frameView = frame->view();
    if (!frameView)
        return std::nullopt;
    return VisibleSelection { frame->visiblePositionForPoint(frameView->windowToContents(windowPoint)) }.toNormalizedRange();
}

void Editor::revealSelectionAfterEditingOperation(const ScrollAlignment& alignment, RevealExtentOption revealExtentOption)
{
    if (m_ignoreSelectionChanges)
        return;

    SelectionRevealMode revealMode = SelectionRevealMode::Reveal;
    Ref document = this->document();
    document->selection().revealSelection(revealMode, alignment, revealExtentOption);
}

void Editor::setIgnoreSelectionChanges(bool ignore, RevealSelection shouldRevealExistingSelection)
{
    if (m_ignoreSelectionChanges == ignore)
        return;

    m_ignoreSelectionChanges = ignore;
#if PLATFORM(IOS_FAMILY)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830> 
    if (!ignore)
        respondToChangedSelection(document().selection().selection(), { });
#endif
    if (!ignore && shouldRevealExistingSelection == RevealSelection::Yes)
        revealSelectionAfterEditingOperation(ScrollAlignment::alignToEdgeIfNeeded, RevealExtentOption::RevealExtent);
}

std::optional<SimpleRange> Editor::compositionRange() const
{
    if (!m_compositionNode)
        return std::nullopt;
    unsigned length = m_compositionNode->length();
    unsigned start = std::min(m_compositionStart, length);
    unsigned end = std::clamp(m_compositionEnd, start, length);
    if (start >= end)
        return std::nullopt;
    return { { { *m_compositionNode, start }, { *m_compositionNode, end } } };
}

bool Editor::getCompositionSelection(unsigned& selectionStart, unsigned& selectionEnd) const
{
    if (!m_compositionNode)
        return false;
    Ref document = this->document();
    const VisibleSelection& selection = document->selection().selection();
    Position start = selection.start();
    if (start.deprecatedNode() != m_compositionNode)
        return false;
    Position end = selection.end();
    if (end.deprecatedNode() != m_compositionNode)
        return false;

    if (static_cast<unsigned>(start.deprecatedEditingOffset()) < m_compositionStart)
        return false;
    if (static_cast<unsigned>(end.deprecatedEditingOffset()) > m_compositionEnd)
        return false;

    selectionStart = start.deprecatedEditingOffset() - m_compositionStart;
    selectionEnd = start.deprecatedEditingOffset() - m_compositionEnd;
    return true;
}

void Editor::transpose()
{
    if (!canEdit())
        return;

    Ref document = this->document();
    VisibleSelection selection = document->selection().selection();
    if (!selection.isCaret())
        return;

    // Make a selection that goes back one character and forward two characters.
    VisiblePosition caret = selection.visibleStart();
    VisiblePosition next = isEndOfParagraph(caret) ? caret : caret.next();
    VisiblePosition previous = next.previous();
    if (next == previous)
        return;
    previous = previous.previous();
    if (!inSameParagraph(next, previous))
        return;
    auto range = makeSimpleRange(previous, next);
    if (!range)
        return;
    VisibleSelection newSelection(*range);

    // Transpose the two characters.
    String text = plainText(*range);
    if (text.length() != 2)
        return;

    // FIXME: This likely won't work with graphemes.
    String transposed = makeString(text[1], text[0]);

    // Select the two characters.
    if (newSelection != document->selection().selection()) {
        if (!document->selection().shouldChangeSelection(newSelection))
            return;
        document->selection().setSelection(newSelection);
    }

    // Insert the transposed characters.
    if (!shouldInsertText(transposed, newSelection.firstRange(), EditorInsertAction::Typed))
        return;
    replaceSelectionWithText(transposed, SelectReplacement::No, SmartReplace::No, EditAction::Insert);
}

void Editor::addRangeToKillRing(const SimpleRange& range, KillRingInsertionMode mode)
{
    addTextToKillRing(plainText(range), mode);
}

void Editor::addTextToKillRing(const String& text, KillRingInsertionMode mode)
{
    if (m_shouldStartNewKillRingSequence)
        killRing().startNewSequence();

    m_shouldStartNewKillRingSequence = false;

    // If the kill was from a backwards motion, prepend to the kill ring.
    // This will ensure that alternating forward and backward kills will
    // build up the original string in the kill ring without permuting it.
    switch (mode) {
    case KillRingInsertionMode::PrependText:
        killRing().prepend(text);
        break;
    case KillRingInsertionMode::AppendText:
        killRing().append(text);
        break;
    }
}

void Editor::startAlternativeTextUITimer()
{
    m_alternativeTextController->startAlternativeTextUITimer(AlternativeTextType::Correction);
}

void Editor::handleAlternativeTextUIResult(const String& correction)
{
    m_alternativeTextController->handleAlternativeTextUIResult(correction);
}


void Editor::dismissCorrectionPanelAsIgnored()
{
    m_alternativeTextController->dismiss(ReasonForDismissingAlternativeText::Ignored);
}

void Editor::changeSelectionAfterCommand(const VisibleSelection& newSelection, OptionSet<FrameSelection::SetSelectionOption> options)
{
    Ref document = this->document();

    if (newSelection.isOrphan() || newSelection.document() != document.ptr())
        return;

    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    bool selectionDidNotChangeDOMPosition = newSelection == document->selection().selection();
    if (selectionDidNotChangeDOMPosition || document->selection().shouldChangeSelection(newSelection))
        document->selection().setSelection(newSelection, options);

    ASSERT_WITH_SECURITY_IMPLICATION(!document->selection().selection().isOrphan());

    // Some editing operations change the selection visually without affecting its position within the DOM.
    // For example when you press return in the following (the caret is marked by ^):
    // <div contentEditable="true"><div>^Hello</div></div>
    // WebCore inserts <div><br></div> *before* the current block, which correctly moves the paragraph down but which doesn't
    // change the caret's DOM position (["hello", 0]). In these situations the above FrameSelection::setSelection call
    // does not call EditorClient::respondToChangedSelection(), which, on the Mac, sends selection change notifications and
    // starts a new kill ring sequence, but we want to do these things (matches AppKit).
#if PLATFORM(IOS_FAMILY)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830>
    if (m_ignoreSelectionChanges)
        return;
#endif
    if (selectionDidNotChangeDOMPosition && client())
        client()->respondToChangedSelection(document->frame());
}

String Editor::selectedText() const
{
    auto options = OptionSet { TextIteratorBehavior::TraversesFlatTree };
    if (!document().quirks().needsToCopyUserSelectNoneQuirk())
        options.add(TextIteratorBehavior::IgnoresUserSelectNone);
    return selectedText(options);
}

String Editor::selectedTextForDataTransfer() const
{
    auto options = OptionSet { TextIteratorBehavior::EmitsImageAltText, TextIteratorBehavior::TraversesFlatTree };
    if (!document().quirks().needsToCopyUserSelectNoneQuirk())
        options.add(TextIteratorBehavior::IgnoresUserSelectNone);
    return selectedText(options);
}

String Editor::selectedText(TextIteratorBehaviors behaviors) const
{
    // We remove '\0' characters because they are not visibly rendered to the user.
    auto range = document().selection().selection().firstRange();
    return range ? makeStringByReplacingAll(plainText(*range, behaviors), '\0', ""_s) : emptyString();
}

RefPtr<TextPlaceholderElement> Editor::insertTextPlaceholder(const IntSize& size)
{
    Ref document = this->document();
    if (document->selection().isNone() || !document->selection().selection().isContentEditable())
        return nullptr;

    // FIXME: Write in terms of replaceSelectionWithFragment(). See <https://bugs.webkit.org/show_bug.cgi?id=208744>.
    deleteSelectionWithSmartDelete(false);

    auto range = document->selection().selection().toNormalizedRange();
    if (!range)
        return nullptr;

    auto placeholder = TextPlaceholderElement::create(document, size);
    createLiveRange(*range)->insertNode(placeholder.copyRef());

    // Inserting the placeholder can run arbitrary JavaScript. Check that it still has a parent.
    if (!placeholder->parentNode())
        return nullptr;

    document->selection().addCaretVisibilitySuppressionReason(CaretVisibilitySuppressionReason::TextPlaceholderIsShowing);

    document->selection().setSelection(VisibleSelection { positionInParentBeforeNode(placeholder.ptr()) }, FrameSelection::defaultSetSelectionOptions(UserTriggered::Yes));

#if ENABLE(WRITING_TOOLS)
    // For Writing Tools, we need the snapshot of the last inserted placeholder.
    if (auto placeholderRange = makeRangeSelectingNode(placeholder.get()))
        protectedDocument()->page()->chrome().client().saveSnapshotOfTextPlaceholderForAnimation(*placeholderRange);
#endif

    return placeholder;
}

void Editor::removeTextPlaceholder(TextPlaceholderElement& placeholder)
{
    ASSERT(placeholder.isConnected());

    Ref document = this->document();

    // Save off state so that we can set the text insertion position to just before the placeholder element after removal.
    RefPtr savedRootEditableElement { placeholder.rootEditableElement() };
    auto savedPositionBeforePlaceholder = positionInParentBeforeNode(&placeholder);

    // FIXME: Save the current selection if it has changed since the placeholder was inserted
    // and restore it after text insertion.
    placeholder.remove();
    // To match the Legacy WebKit implementation, set the text insertion point to be before where the placeholder used to be.
    if (document->selection().isFocusedAndActive() && document->focusedElement() == savedRootEditableElement)
        document->selection().setSelection(VisibleSelection { savedPositionBeforePlaceholder }, FrameSelection::defaultSetSelectionOptions(UserTriggered::Yes));

    document->selection().removeCaretVisibilitySuppressionReason(CaretVisibilitySuppressionReason::TextPlaceholderIsShowing);
}

static inline void collapseCaretWidth(IntRect& rect)
{
    // FIXME: Width adjustment doesn't work for rotated text.
    if (rect.width() == caretWidth())
        rect.setWidth(0);
    else if (rect.height() == caretWidth())
        rect.setHeight(0);
}

IntRect Editor::firstRectForRange(const SimpleRange& range) const
{
    range.start.protectedDocument()->updateLayout();

    VisiblePosition start(makeDeprecatedLegacyPosition(range.start));

    if (range.collapsed()) {
        // FIXME: Getting caret rect and removing caret width is a very roundabout way to get collapsed range location.
        // In particular, width adjustment doesn't work for rotated text.
        IntRect startCaretRect = RenderedPosition(start).absoluteRect();
        collapseCaretWidth(startCaretRect);
        return startCaretRect;
    }

    VisiblePosition end(makeDeprecatedLegacyPosition(range.end), Affinity::Upstream);

    if (inSameLine(start, end))
        return enclosingIntRect(unitedBoundingBoxes(RenderObject::absoluteTextQuads(range)));

    return RenderedPosition(start).absoluteRect(CaretRectMode::ExpandToEndOfLine);
}

bool Editor::shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, Affinity affinity, bool stillSelecting) const
{
#if PLATFORM(IOS_FAMILY)
    if (document().frame() && document().frame()->selectionChangeCallbacksDisabled())
        return true;
#endif
    return client() && client()->shouldChangeSelectedRange(oldSelection.toNormalizedRange(), newSelection.toNormalizedRange(), affinity, stillSelecting);
}

void Editor::computeAndSetTypingStyle(EditingStyle& style, EditAction editingAction)
{
    Ref document = this->document();
    if (style.isEmpty()) {
        document->selection().clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtr<EditingStyle> typingStyle;
    if (auto existingTypingStyle = document->selection().typingStyle())
        typingStyle = existingTypingStyle->copy();
    else
        typingStyle = EditingStyle::create();
    typingStyle->overrideTypingStyleAt(style, document->selection().selection().visibleStart().deepEquivalent());

    // Handle block styles, substracting these from the typing style.
    RefPtr<EditingStyle> blockStyle = typingStyle->extractAndRemoveBlockProperties();
    if (!blockStyle->isEmpty())
        ApplyStyleCommand::create(document.copyRef(), blockStyle.get(), editingAction)->apply();

    // Set the remaining style as the typing style.
    document->selection().setTypingStyle(WTFMove(typingStyle));
}

void Editor::computeAndSetTypingStyle(StyleProperties& properties, EditAction editingAction)
{
    return computeAndSetTypingStyle(EditingStyle::create(&properties), editingAction);
}

void Editor::textFieldDidBeginEditing(Element& e)
{
    if (client())
        client()->textFieldDidBeginEditing(e);
}

void Editor::textFieldDidEndEditing(Element& e)
{
    dismissCorrectionPanelAsIgnored();
    if (client())
        client()->textFieldDidEndEditing(e);
}

void Editor::textDidChangeInTextField(Element& e)
{
    if (client())
        client()->textDidChangeInTextField(e);
}

bool Editor::doTextFieldCommandFromEvent(Element& e, KeyboardEvent* ke)
{
    if (client())
        return client()->doTextFieldCommandFromEvent(e, ke);

    return false;
}

void Editor::textWillBeDeletedInTextField(Element& input)
{
    if (client())
        client()->textWillBeDeletedInTextField(input);
}

void Editor::textDidChangeInTextArea(Element& e)
{
    if (client())
        client()->textDidChangeInTextArea(e);
}

void Editor::applyEditingStyleToBodyElement() const
{
    RefPtr body { document().body() };
    if (!body)
        return;
    body->setInlineStyleProperty(CSSPropertyOverflowWrap, CSSValueBreakWord);
    body->setInlineStyleProperty(CSSPropertyWebkitNbspMode, CSSValueSpace);
    body->setInlineStyleProperty(CSSPropertyLineBreak, CSSValueAfterWhiteSpace);
}

bool Editor::findString(const String& target, FindOptions options)
{
    Ref document = this->document();
    std::optional<SimpleRange> resultRange;
    {
        document->updateLayoutIgnorePendingStylesheets();
        Style::PostResolutionCallbackDisabler disabler(document);
        VisibleSelection selection = document->selection().selection();
        resultRange = rangeOfString(target, selection.firstRange(), options);
    }

    if (!resultRange)
        return false;

    if (!options.contains(FindOption::DoNotSetSelection))
        document->selection().setSelection(VisibleSelection(*resultRange));

    if (!(options.contains(FindOption::DoNotRevealSelection)))
        document->selection().revealSelection();

    return true;
}

template<typename T> static auto& start(T& range, FindOptions options)
{
    return options.contains(FindOption::Backwards) ? range.end : range.start;
}

template<typename T> static auto& end(T& range, FindOptions options)
{
    return options.contains(FindOption::Backwards) ? range.start : range.end;
}

static BoundaryPoint makeBoundaryPointAfterNodeContents(Node& node, FindOptions options)
{
    return options.contains(FindOption::Backwards) ? makeBoundaryPointBeforeNodeContents(node) : makeBoundaryPointAfterNodeContents(node);
}

static std::optional<BoundaryPoint> makeBoundaryPointAfterNode(Node& node, FindOptions options)
{
    return options.contains(FindOption::Backwards) ? makeBoundaryPointBeforeNode(node) : makeBoundaryPointAfterNode(node);
}

static SimpleRange collapseIfRootsDiffer(SimpleRange&& range)
{
    // FIXME: This helps correct results in some cases involving shadow trees. But we can incorrectly find a string with middle characters in an input element and first and last characters outside it.
    return &range.start.container->rootNode() == &range.end.container->rootNode()
        ? WTFMove(range) : SimpleRange { range.start, range.start };
}

std::optional<SimpleRange> Editor::rangeOfString(const String& target, const std::optional<SimpleRange>& referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return std::nullopt;

    // Start from an edge of the reference range, if there's a reference range that's not in shadow content. Which edge
    // is used depends on whether we're searching forward or backward, and whether startInSelection is set.

    bool startInReferenceRange = referenceRange && options.contains(FindOption::StartInSelection);
    auto shadowTreeRoot = referenceRange ? referenceRange->startContainer().containingShadowRoot() : nullptr;

    Ref document = this->document();
    auto searchRange = makeRangeSelectingNodeContents(document);
    if (referenceRange)
        start(searchRange, options) = startInReferenceRange ? start(*referenceRange, options) : end(*referenceRange, options);
    if (shadowTreeRoot)
        end(searchRange, options) = makeBoundaryPointAfterNodeContents(*shadowTreeRoot, options);
    auto resultRange = collapseIfRootsDiffer(findPlainText(searchRange, target, options));

    // If we started in the reference range and the found range exactly matches the reference range, find again.
    // Build a selection with the found range to remove collapsed whitespace.
    // Compare ranges instead of selection objects to ignore the way that the current selection was made.
    if (startInReferenceRange && VisibleSelection(resultRange).toNormalizedRange() == referenceRange) {
        searchRange = makeRangeSelectingNodeContents(document);
        start(searchRange, options) = end(*referenceRange, options);
        if (shadowTreeRoot)
            end(searchRange, options) = makeBoundaryPointAfterNodeContents(*shadowTreeRoot, options);
        resultRange = collapseIfRootsDiffer(findPlainText(searchRange, target, options));
    }

    // If nothing was found in the shadow tree, search in main content following the shadow tree.
    if (resultRange.collapsed() && shadowTreeRoot) {
        searchRange = makeRangeSelectingNodeContents(document);
        if (auto host = shadowTreeRoot->shadowHost())
            start(searchRange, options) = *makeBoundaryPointAfterNode(*host, options);
        resultRange = collapseIfRootsDiffer(findPlainText(searchRange, target, options));
    }

    // If we didn't find anything and we're wrapping, search again in the entire document (this will
    // redundantly re-search the area already searched in some cases).
    if (resultRange.collapsed() && options.contains(FindOption::WrapAround)) {
        resultRange = collapseIfRootsDiffer(findPlainText(makeRangeSelectingNodeContents(document), target, options));
        // We used to return false here if we ended up with the same range that we started with
        // (e.g., the reference range was already the only instance of this text). But we decided that
        // this should be a success case instead, so we'll just fall through in that case.
    }

    return resultRange.collapsed() ? std::nullopt : std::make_optional(resultRange);
}

static bool isFrameInRange(LocalFrame& frame, const SimpleRange& range)
{
    for (RefPtr ownerElement = frame.ownerElement(); ownerElement; ownerElement = ownerElement->document().ownerElement()) {
        if (&ownerElement->document() == &range.start.document())
            return intersects<ComposedTree>(range, *ownerElement);
    }
    return false;
}

unsigned Editor::countMatchesForText(const String& target, const std::optional<SimpleRange>& range, FindOptions options, unsigned limit, bool markMatches, Vector<SimpleRange>* matches)
{
    if (target.isEmpty())
        return 0;

    std::optional<SimpleRange> searchRange;
    Ref document = this->document();
    if (range) {
        if (&range->start.document() == document.ptr())
            searchRange = *range;
        else if (!isFrameInRange(*document->frame(), *range))
            return 0;
    }
    if (!searchRange)
        searchRange = makeRangeSelectingNodeContents(document);

    auto originalEnd = searchRange->end;

    unsigned matchCount = 0;
    do {
        auto resultRange = findPlainText(*searchRange, target, options - FindOption::Backwards);
        if (resultRange.collapsed()) {
            if (!resultRange.start.container->isInShadowTree())
                break;

            searchRange->start = makeBoundaryPointAfterNodeContents(*resultRange.start.container->shadowHost());
            searchRange->end = originalEnd;
            continue;
        }

        ++matchCount;
        if (matches)
            matches->append(resultRange);

        if (markMatches)
            addMarker(resultRange, DocumentMarkerType::TextMatch);

        // Stop looking if we hit the specified limit. A limit of 0 means no limit.
        if (limit > 0 && matchCount >= limit)
            break;

        // Set the new start for the search range to be the end of the previous result range.
        // There is no need to use VisiblePosition here: findPlainText will use TextIterator to go over visible text nodes.
        searchRange->start = WTFMove(resultRange.end);

        if (searchRange->collapsed()) {
            if (auto shadowTreeRoot = searchRange->start.container->containingShadowRoot())
                searchRange->end = makeBoundaryPointAfterNodeContents(*shadowTreeRoot);
        }
    } while (true);

    return matchCount;
}

void Editor::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == m_areMarkedTextMatchesHighlighted)
        return;

    m_areMarkedTextMatchesHighlighted = flag;
    if (CheckedPtr markers = document().markersIfExists())
        markers->repaintMarkers(DocumentMarkerType::TextMatch);
}

#if !PLATFORM(MAC)
void Editor::selectionWillChange()
{
}
#endif

void Editor::respondToChangedSelection(const VisibleSelection&, OptionSet<FrameSelection::SetSelectionOption> options)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: Should suppress selection change notifications during a composition change <https://webkit.org/b/38830> 
    if (m_ignoreSelectionChanges)
        return;
#endif

    Ref document = this->document();

#if PLATFORM(IOS_FAMILY)
    auto continueDisplayingSuggestion = [&] {
        if (!m_writingSuggestionData)
            return false;

        auto newSelection = document->selection().selection();

        auto range = newSelection.firstRange();
        if (!range)
            return false;

        range->start.offset = 0;

        auto completion = makeString(m_writingSuggestionData->originalPrefix(), m_writingSuggestionData->originalSuffix());
        auto content = plainTextReplacingNoBreakSpace(*range);

        auto currentFullText = makeString(content, m_writingSuggestionData->content());

        if (completion == content)
            return false;

        if (completion != currentFullText)
            return false;

        if (content.length() <= m_writingSuggestionData->originalPrefix().length())
            return false;

        return completion.startsWith(content);
    }();

    if (m_writingSuggestionData && !continueDisplayingSuggestion)
        removeWritingSuggestionIfNeeded();
#endif

    if (client())
        client()->respondToChangedSelection(document->frame());

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS_FAMILY)
    if (shouldDetectTelephoneNumbers())
        m_telephoneNumberDetectionUpdateTimer.restart();
#endif

    setStartNewKillRingSequence(true);
    m_imageElementsToLoadBeforeRevealingSelection.clear();

    if (!m_hasHandledAnyEditing && !document->hasHadUserInteraction() && !document->isTopDocument())
        return;

    if (m_editorUIUpdateTimer.isActive())
        return;

    // Don't check spelling and grammar if the change of selection is triggered by spelling correction itself.
    m_editorUIUpdateTimerShouldCheckSpellingAndGrammar = options.contains(FrameSelection::SetSelectionOption::CloseTyping) && !options.contains(FrameSelection::SetSelectionOption::SpellCorrectionTriggered);
    m_editorUIUpdateTimerWasTriggeredByDictation = options.contains(FrameSelection::SetSelectionOption::DictationTriggered);
    scheduleEditorUIUpdate();
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

bool Editor::shouldDetectTelephoneNumbers() const
{
    return document().isTelephoneNumberParsingEnabled() && TelephoneNumberDetector::isSupported();
}

static Vector<SimpleRange> scanForTelephoneNumbers(const SimpleRange& range)
{
    // Don't scan for phone numbers inside editable regions.
    if (Ref startNode = range.startContainer(); startNode->hasEditableStyle())
        return { };

    if (ImageOverlay::isInsideOverlay(range))
        return { };

    auto text = plainText(range);
    Vector<SimpleRange> result;
    int relativeStartPosition = 0;
    int relativeEndPosition = 0;
    auto characters = StringView { text }.upconvertedCharacters();
    auto span = characters.span();
    while (!span.empty() && TelephoneNumberDetector::find(span, &relativeStartPosition, &relativeEndPosition)) {
        auto scannerPosition = span.data() - characters.span().data();
        ASSERT(scannerPosition + relativeEndPosition <= text.length());
        result.append(resolveCharacterRange(range, CharacterRange(scannerPosition + relativeStartPosition, relativeEndPosition - relativeStartPosition)));
        skip(span, relativeEndPosition);
    }
    return result;
}

static std::optional<SimpleRange> extendSelection(const SimpleRange& range, unsigned charactersToExtend)
{
    auto start = makeDeprecatedLegacyPosition(range.start);
    auto end = makeDeprecatedLegacyPosition(range.end);
    for (unsigned i = 0; i < charactersToExtend; ++i) {
        start = start.previous(Character);
        end = end.next(Character);
    }
    return makeSimpleRange(start, end);
}

void Editor::scanSelectionForTelephoneNumbers()
{
    // FIXME: Why is it helpful here to check client for null?
    if (!shouldDetectTelephoneNumbers() || !client())
        return;

    m_detectedTelephoneNumberRanges.clear();
    
    auto notifyController = makeScopeExit([&] {
        if (RefPtr page = document().page())
            page->protectedServicesOverlayController()->selectedTelephoneNumberRangesChanged();
    });

    auto selection = document().selection().selection();
    if (!selection.isRange())
        return;

    auto selectedRange = selection.firstRange();
    if (!selectedRange)
        return;

    // Extend the range a few characters in each direction to detect incompletely selected phone numbers.
    constexpr unsigned charactersToExtend = 15;
    auto extendedRange = extendSelection(*selectedRange, charactersToExtend);
    if (!extendedRange)
        return;

    for (auto& range : scanForTelephoneNumbers(*extendedRange)) {
        // FIXME: Why do we do this unconditionally instead of when only when it overlaps the selection?
        addMarker(range, DocumentMarkerType::TelephoneNumber);

        // Only consider ranges with a detected telephone number if they overlap with the selection.
        if (intersects<ComposedTree>(range, *selectedRange))
            m_detectedTelephoneNumberRanges.append(range);
    }
}

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION) && !PLATFORM(IOS_FAMILY)

void Editor::updateEditorUINowIfScheduled()
{
    if (!m_editorUIUpdateTimer.isActive())
        return;
    m_editorUIUpdateTimer.stop();
    editorUIUpdateTimerFired();
}

void Editor::editorUIUpdateTimerFired()
{
    VisibleSelection oldSelection = m_oldSelectionForEditorUIUpdate;

    m_alternativeTextController->stopPendingCorrection(oldSelection);
    
    bool isContinuousSpellCheckingEnabled = this->isContinuousSpellCheckingEnabled();
    bool isContinuousGrammarCheckingEnabled = isContinuousSpellCheckingEnabled && isGrammarCheckingEnabled();
    Ref document = this->document();
    if (isContinuousSpellCheckingEnabled) {
        VisibleSelection newAdjacentWords;
        VisibleSelection newSelectedSentence;
        bool caretBrowsing = document->settings().caretBrowsingEnabled();
        if (document->selection().selection().isContentEditable() || caretBrowsing) {
            VisiblePosition newStart(document->selection().selection().visibleStart());
#if !PLATFORM(IOS_FAMILY)
            newAdjacentWords = VisibleSelection(startOfWord(newStart, WordSide::LeftWordIfOnBoundary), endOfWord(newStart, WordSide::RightWordIfOnBoundary));
#else
            // If this bug gets fixed, this PLATFORM(IOS_FAMILY) code could be removed:
            // <rdar://problem/7259611> Word boundary code on iPhone gives different results than desktop
            WordSide startWordSide = WordSide::LeftWordIfOnBoundary;
            char32_t c = newStart.characterBefore();
            // FIXME: VisiblePosition::characterAfter() and characterBefore() do not emit newlines the same
            // way as TextIterator, so we do an isStartOfParagraph check here.
            if (deprecatedIsSpaceOrNewline(c) || c == noBreakSpace || isStartOfParagraph(newStart)) {
                startWordSide = WordSide::RightWordIfOnBoundary;
            }
            newAdjacentWords = VisibleSelection(startOfWord(newStart, startWordSide), endOfWord(newStart, WordSide::RightWordIfOnBoundary));
#endif // !PLATFORM(IOS_FAMILY)
            if (isContinuousGrammarCheckingEnabled)
                newSelectedSentence = VisibleSelection(startOfSentence(newStart), endOfSentence(newStart));
        }

        // When typing we check spelling elsewhere, so don't redo it here.
        // If this is a change in selection resulting from a delete operation,
        // oldSelection may no longer be in the document.
        if (m_editorUIUpdateTimerShouldCheckSpellingAndGrammar && oldSelection.isContentEditable() && oldSelection.start().deprecatedNode() && oldSelection.start().anchorNode()->isConnected()) {
            VisiblePosition oldStart(oldSelection.visibleStart());
            VisibleSelection oldAdjacentWords = VisibleSelection(startOfWord(oldStart, WordSide::LeftWordIfOnBoundary), endOfWord(oldStart, WordSide::RightWordIfOnBoundary));
            if (oldAdjacentWords != newAdjacentWords) {
                if (isContinuousGrammarCheckingEnabled) {
                    VisibleSelection oldSelectedSentence = VisibleSelection(startOfSentence(oldStart), endOfSentence(oldStart));
                    markMisspellingsAndBadGrammar(oldAdjacentWords, oldSelectedSentence != newSelectedSentence, oldSelectedSentence);
                } else
                    markMisspellingsAndBadGrammar(oldAdjacentWords, false, oldAdjacentWords);
            }
        }

        if (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingType::Spelling)) {
            if (auto wordRange = newAdjacentWords.toNormalizedRange())
                removeMarkers(*wordRange, DocumentMarkerType::Spelling);
        }
        if (!textChecker() || textChecker()->shouldEraseMarkersAfterChangeSelection(TextCheckingType::Grammar)) {
            if (auto sentenceRange = newSelectedSentence.toNormalizedRange())
                removeMarkers(*sentenceRange, DocumentMarkerType::Grammar);
        }
    }

    // When continuous spell checking is off, existing markers disappear after the selection changes.
    if (CheckedPtr markers = document->markersIfExists()) {
        if (!isContinuousSpellCheckingEnabled)
            markers->removeMarkers(DocumentMarkerType::Spelling);
        if (!isContinuousGrammarCheckingEnabled)
            markers->removeMarkers(DocumentMarkerType::Grammar);
    }

    if (!m_editorUIUpdateTimerWasTriggeredByDictation)
        m_alternativeTextController->respondToChangedSelection(oldSelection);

    m_oldSelectionForEditorUIUpdate = document->selection().selection();

#if ENABLE(ATTACHMENT_ELEMENT)
    notifyClientOfAttachmentUpdates();
#endif
}

static RefPtr<Node> findFirstMarkable(Node* startingNode)
{
    RefPtr node { startingNode };
    while (node) {
        {
            ScriptDisallowedScope::InMainThread scriptDisallowedScope;
            if (!node->renderer())
                return nullptr;
            if (node->renderer()->isRenderTextOrLineBreak())
                return node;
        }
        if (RefPtr element = dynamicDowncast<HTMLTextFormControlElement>(*node); element && element->isTextField())
            node = element->visiblePositionForIndex(1).deepEquivalent().deprecatedNode();
        else if (node->firstChild())
            node = node->firstChild();
        else
            node = node->nextSibling();
    }

    return nullptr;
}

bool Editor::selectionStartHasMarkerFor(DocumentMarkerType markerType, int from, int length) const
{
    auto node = findFirstMarkable(document().selection().selection().start().protectedDeprecatedNode().get());
    if (!node)
        return false;

    CheckedPtr markers = document().markersIfExists();
    if (!markers)
        return false;

    unsigned startOffset = static_cast<unsigned>(from);
    unsigned endOffset = static_cast<unsigned>(from + length);
    for (auto& marker : markers->markersFor(*node)) {
        if (marker->startOffset() <= startOffset && endOffset <= marker->endOffset() && marker->type() == markerType)
            return true;
    }

    return false;
}

void Editor::selectionStartSetMarkerForTesting(DocumentMarkerType markerType, int from, int length, const String& data)
{
    RefPtr node = findFirstMarkable(document().selection().selection().start().protectedDeprecatedNode().get());
    if (!node)
        return;

    RefPtr text = dynamicDowncast<Text>(node);
    if (!text)
        return;

    CheckedRef markers = document().markers();

    unsigned unsignedFrom = static_cast<unsigned>(from);
    unsigned unsignedLength = static_cast<unsigned>(length);

    switch (markerType) {
    case DocumentMarkerType::TransparentContent:
        markers->addMarker(*text, unsignedFrom, unsignedLength, markerType, DocumentMarker::TransparentContentData { node, WTF::UUID { 0 } });
        return;

    case DocumentMarkerType::DraggedContent:
        markers->addMarker(*text, unsignedFrom, unsignedLength, markerType, node);
        return;

    case DocumentMarkerType::Grammar:
    case DocumentMarkerType::Spelling:
    case DocumentMarkerType::Replacement:
        markers->addMarker(*text, unsignedFrom, unsignedLength, markerType, data);
        return;

    default:
        // FIXME: Support more marker types in this testing utility function.
        RELEASE_ASSERT_NOT_REACHED();
    }
}

OptionSet<TextCheckingType> Editor::resolveTextCheckingTypeMask(const Node& rootEditableElement, OptionSet<TextCheckingType> textCheckingOptions)
{
#if USE(AUTOMATIC_TEXT_REPLACEMENT) && !PLATFORM(IOS_FAMILY)
    bool onlyAllowsTextReplacement = false;
    if (RefPtr host = dynamicDowncast<HTMLInputElement>(rootEditableElement.shadowHost()))
        onlyAllowsTextReplacement = host->isSpellcheckDisabledExceptTextReplacement();
    if (onlyAllowsTextReplacement)
        textCheckingOptions = textCheckingOptions & TextCheckingType::Replacement;
#else
    UNUSED_PARAM(rootEditableElement);
#endif

    bool shouldMarkSpelling = textCheckingOptions.contains(TextCheckingType::Spelling);
    bool shouldMarkGrammar = textCheckingOptions.contains(TextCheckingType::Grammar);
#if !PLATFORM(IOS_FAMILY)
    bool shouldShowCorrectionPanel = textCheckingOptions.contains(TextCheckingType::ShowCorrectionPanel);
    bool shouldCheckForCorrection = shouldShowCorrectionPanel || textCheckingOptions.contains(TextCheckingType::Correction);
#else
    bool shouldCheckForCorrection = textCheckingOptions.contains(TextCheckingType::Correction);
#endif

    OptionSet<TextCheckingType> checkingTypes;
    if (shouldMarkSpelling)
        checkingTypes.add(TextCheckingType::Spelling);
    if (shouldMarkGrammar)
        checkingTypes.add(TextCheckingType::Grammar);
    if (shouldCheckForCorrection)
        checkingTypes.add(TextCheckingType::Correction);
#if !PLATFORM(IOS_FAMILY)
    if (shouldShowCorrectionPanel)
        checkingTypes.add(TextCheckingType::ShowCorrectionPanel);

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    bool shouldPerformReplacement = textCheckingOptions.contains(TextCheckingType::Replacement);
    if (shouldPerformReplacement) {
        if (!onlyAllowsTextReplacement) {
            if (isAutomaticLinkDetectionEnabled())
                checkingTypes.add(TextCheckingType::Link);
            if (isAutomaticQuoteSubstitutionEnabled())
                checkingTypes.add(TextCheckingType::Quote);
            if (isAutomaticDashSubstitutionEnabled())
                checkingTypes.add(TextCheckingType::Dash);
            if (shouldMarkSpelling && isAutomaticSpellingCorrectionEnabled())
                checkingTypes.add(TextCheckingType::Correction);
        }
        if (isAutomaticTextReplacementEnabled())
            checkingTypes.add(TextCheckingType::Replacement);
    }
#endif
#endif // !PLATFORM(IOS_FAMILY)

    return checkingTypes;
}

static bool candidateWouldReplaceText(const VisibleSelection& selection)
{
    // If the character behind the caret in the current selection is anything but a space or a newline then we should
    // replace the whole current word with the candidate.
    char32_t characterAfterSelection, characterBeforeSelection, twoCharacterBeforeSelection = 0;
    charactersAroundPosition(selection.visibleStart(), characterAfterSelection, characterBeforeSelection, twoCharacterBeforeSelection);
    return !(characterBeforeSelection == '\0' || characterBeforeSelection == '\n' || characterBeforeSelection == ' ');
}

String Editor::stringForCandidateRequest() const
{
    auto& selection = document().selection().selection();
    auto range = selection.isCaret() ? wordRangeFromPosition(selection.start()) : selection.toNormalizedRange();
    if (!range)
        return { };
    if (!candidateWouldReplaceText(selection))
        return { };
    return plainText(*range);
}

std::optional<SimpleRange> Editor::contextRangeForCandidateRequest() const
{
    auto& selection = document().selection().selection();
    return makeSimpleRange(startOfParagraph(selection.visibleStart()), endOfParagraph(selection.visibleEnd()));
}

std::optional<SimpleRange> Editor::rangeForTextCheckingResult(const TextCheckingResult& result) const
{
    if (!result.range.length)
        return std::nullopt;
    auto contextRange = contextRangeForCandidateRequest();
    if (!contextRange)
        return std::nullopt;
    return resolveCharacterRange(*contextRange, result.range);
}

void Editor::scheduleEditorUIUpdate()
{
    m_editorUIUpdateTimer.startOneShot(0_s);
}

#if !PLATFORM(COCOA)

String Editor::platformContentTypeForBlobType(const String& type) const
{
    return type;
}

#endif

static Vector<TextList> editableTextListsAtPositionInDescendingOrder(const Position& position)
{
    RefPtr startContainer { position.containerNode() };
    if (!startContainer)
        return { };

    auto editableRoot = highestEditableRoot(firstPositionInOrBeforeNode(startContainer.get()));
    if (!editableRoot)
        return { };

    Vector<Ref<HTMLElement>> enclosingLists;
    for (auto& ancestor : ancestorsOfType<HTMLElement>(*startContainer)) {
        if (&ancestor == editableRoot.get())
            break;

        if (!ancestor.renderer())
            continue;

        if (is<HTMLUListElement>(ancestor) || is<HTMLOListElement>(ancestor))
            enclosingLists.append(ancestor);
    }

    Vector<TextList> textLists;
    textLists.reserveInitialCapacity(enclosingLists.size());
    for (auto iterator = enclosingLists.rbegin(); iterator != enclosingLists.rend(); ++iterator) {
        auto& list = iterator->get();
        auto* orderedList = dynamicDowncast<HTMLOListElement>(list);
        if (!list.renderer())
            continue;
        auto style = list.renderer()->style().listStyleType();
        textLists.append({ style, orderedList ? orderedList->start() : 1, !!orderedList });
    }

    return textLists;
}

FontAttributes Editor::fontAttributesAtSelectionStart()
{
    FontAttributes attributes;
    attributes.font = fontForSelection(attributes.hasMultipleFonts);

    RefPtr<Node> nodeToRemove;
    auto nodeRemovalScope = makeScopeExit([&nodeToRemove]() {
        if (nodeToRemove)
            nodeToRemove->remove();
    });

    auto* style = styleForSelectionStart(nodeToRemove);
    if (!style)
        return { };

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    // FIXME: for now, always report the colors after applying -apple-color-filter. In future not all clients
    // may want this, so we may have to add a setting to control it. See also editingAttributedStringFromRange().
    auto backgroundColor = style->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (backgroundColor.isVisible())
        attributes.backgroundColor = backgroundColor;

    auto foregroundColor = style->visitedDependentColorWithColorFilter(CSSPropertyColor);
    // FIXME: isBlackColor not suitable for dark mode.
    if (foregroundColor.isValid() && !Color::isBlackColor(foregroundColor))
        attributes.foregroundColor = foregroundColor;

    attributes.fontShadow = WTF::switchOn(style->textShadow(),
        [&](const CSS::Keyword::None&) {
            return FontShadow { };
        },
        [&](const auto& shadows) {
            return FontShadow {
                style->colorWithColorFilter(shadows[0].color),
                { shadows[0].location.x().value, shadows[0].location.y().value },
                shadows[0].blur.value
            };
        }
    );

    attributes.subscriptOrSuperscript = WTF::switchOn(style->verticalAlign(),
        [](const CSS::Keyword::Baseline&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::Sub&) { return FontAttributes::SubscriptOrSuperscript::Subscript; },
        [](const CSS::Keyword::Super&) { return FontAttributes::SubscriptOrSuperscript::Superscript; },
        [](const CSS::Keyword::Bottom&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::Middle&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::TextBottom&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::TextTop&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::Top&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const CSS::Keyword::WebkitBaselineMiddle&) { return FontAttributes::SubscriptOrSuperscript::None; },
        [](const Style::VerticalAlign::Length&) { return FontAttributes::SubscriptOrSuperscript::None; }
    );

    attributes.textLists = editableTextListsAtPositionInDescendingOrder(document().selection().selection().start());

    switch (style->textAlign()) {
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        attributes.horizontalAlignment = FontAttributes::HorizontalAlignment::Right;
        break;
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        attributes.horizontalAlignment = FontAttributes::HorizontalAlignment::Left;
        break;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        attributes.horizontalAlignment = FontAttributes::HorizontalAlignment::Center;
        break;
    case TextAlignMode::Justify:
        attributes.horizontalAlignment = FontAttributes::HorizontalAlignment::Justify;
        break;
    case TextAlignMode::Start:
        if (style->hasExplicitlySetDirection())
            attributes.horizontalAlignment = style->isLeftToRightDirection() ? FontAttributes::HorizontalAlignment::Left : FontAttributes::HorizontalAlignment::Right;
        else
            attributes.horizontalAlignment = FontAttributes::HorizontalAlignment::Natural;
        break;
    case TextAlignMode::End:
        attributes.horizontalAlignment = style->isLeftToRightDirection() ? FontAttributes::HorizontalAlignment::Right : FontAttributes::HorizontalAlignment::Left;
        break;
    }

    RefPtr typingStyle { document().selection().typingStyle() };
    if (typingStyle && typingStyle->style()) {
        if (RefPtr value = dynamicDowncast<CSSValueList>(typingStyle->style()->getPropertyCSSValue(CSSPropertyWebkitTextDecorationsInEffect))) {
            if (value->hasValue(CSSValueLineThrough))
                attributes.hasStrikeThrough = true;
            if (value->hasValue(CSSValueUnderline))
                attributes.hasUnderline = true;
        }
    } else {
        auto decoration = style->textDecorationLineInEffect();
        if (decoration & TextDecorationLine::LineThrough)
            attributes.hasStrikeThrough = true;
        if (decoration & TextDecorationLine::Underline)
            attributes.hasUnderline = true;
    }

    return attributes;
}

#if ENABLE(ATTACHMENT_ELEMENT)

PromisedAttachmentInfo Editor::promisedAttachmentInfo(Element& element)
{
    auto* client = this->client();
    if (!client || !client->supportsClientSideAttachmentData())
        return { };

    RefPtr<HTMLAttachmentElement> attachment;
    if (auto* possibleAttachment = dynamicDowncast<HTMLAttachmentElement>(element))
        attachment = possibleAttachment;
    else if (auto* image = dynamicDowncast<HTMLImageElement>(element))
        attachment = image->attachmentElement();

    if (!attachment)
        return { };

    Vector<std::pair<String, RefPtr<WebCore::SharedBuffer>>>  additionalTypesAndData;
#if PLATFORM(COCOA)
    getPasteboardTypesAndDataForAttachment(element, additionalTypesAndData);
#endif

    return { attachment->uniqueIdentifier(), WTFMove(additionalTypesAndData) };
}

void Editor::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& preferredFileName, Ref<FragmentedSharedBuffer>&& data)
{
    if (auto* client = this->client())
        client->registerAttachmentIdentifier(identifier, contentType, preferredFileName, WTFMove(data));
}

void Editor::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& filePath)
{
    if (auto* client = this->client())
        client->registerAttachmentIdentifier(identifier, contentType, filePath);
}

void Editor::registerAttachments(Vector<SerializedAttachmentData>&& data)
{
    if (auto* client = this->client())
        client->registerAttachments(WTFMove(data));
}

void Editor::registerAttachmentIdentifier(const String& identifier, const AttachmentAssociatedElement& element)
{
    auto* client = this->client();
    if (!client)
        return;

    auto attachmentInfo = [&]() -> std::optional<std::tuple<String, String, Ref<FragmentedSharedBuffer>>> {
        RefPtr imageElement = dynamicDowncast<HTMLImageElement>(element.asHTMLElement());
        if (!imageElement)
            return std::nullopt;

        CheckedPtr renderer = dynamicDowncast<RenderImage>(imageElement->renderer());
        if (!renderer)
            return std::nullopt;

        auto* cachedImage = renderer->cachedImage();
        if (!cachedImage || cachedImage->errorOccurred())
            return std::nullopt;

        String contentType;
        if (auto* image = cachedImage->image())
            contentType = image->mimeType();

        if (contentType.isEmpty())
            return std::nullopt;

        RefPtr imageData = cachedImage->resourceBuffer();
        if (!imageData)
            return std::nullopt;

        String name = imageElement->attributeWithoutSynchronization(altAttr);
        if (name.isEmpty())
            name = imageElement->document().completeURL(imageElement->imageSourceURL()).lastPathComponent().toString();

        if (name.isEmpty())
            return std::nullopt;

        return { { WTFMove(contentType), WTFMove(name), imageData.releaseNonNull() } };
    }();

    if (attachmentInfo) {
        auto& [contentType, preferredFileName, data] = *attachmentInfo;
        client->registerAttachmentIdentifier(identifier, WTFMove(contentType), WTFMove(preferredFileName), WTFMove(data));
    } else
        client->registerAttachmentIdentifier(identifier);
}

void Editor::cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier)
{
    if (auto* client = this->client())
        client->cloneAttachmentData(fromIdentifier, toIdentifier);
}

void Editor::didInsertAttachmentElement(HTMLAttachmentElement& attachment)
{
    auto identifier = attachment.uniqueIdentifier();
    if (identifier.isEmpty())
        return;

    if (!m_removedAttachmentIdentifiers.take(identifier))
        m_insertedAttachmentIdentifiers.add(identifier);
    scheduleEditorUIUpdate();
}

void Editor::didRemoveAttachmentElement(HTMLAttachmentElement& attachment)
{
    auto identifier = attachment.uniqueIdentifier();
    if (identifier.isEmpty())
        return;

    if (!m_insertedAttachmentIdentifiers.take(identifier))
        m_removedAttachmentIdentifiers.add(identifier);
    scheduleEditorUIUpdate();
}

void Editor::notifyClientOfAttachmentUpdates()
{
    auto removedAttachmentIdentifiers = WTFMove(m_removedAttachmentIdentifiers);
    auto insertedAttachmentIdentifiers = WTFMove(m_insertedAttachmentIdentifiers);
    if (!client())
        return;

    for (auto& identifier : removedAttachmentIdentifiers)
        client()->didRemoveAttachmentWithIdentifier(identifier);

    for (auto& identifier : insertedAttachmentIdentifiers) {
        if (auto attachment = document().attachmentForIdentifier(identifier))
            client()->didInsertAttachmentWithIdentifier(identifier, attachment->attributeWithoutSynchronization(HTMLNames::srcAttr), attachment->associatedElementType());
        else
            ASSERT_NOT_REACHED();
    }
}

void Editor::insertAttachment(const String& identifier, std::optional<uint64_t>&& fileSize, const AtomString& fileName, const AtomString& contentType)
{
    Ref document = this->document();
    auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
    attachment->setUniqueIdentifier(identifier);
    attachment->updateAttributes(WTFMove(fileSize), contentType, fileName);

    auto fragmentToInsert = document->createDocumentFragment();
    fragmentToInsert->appendChild(attachment.get());

    replaceSelectionWithFragment(fragmentToInsert.get(), SelectReplacement::No, SmartReplace::No, MatchStyle::Yes);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void Editor::handleAcceptedCandidate(TextCheckingResult acceptedCandidate)
{
    const VisibleSelection& selection = document().selection().selection();

    m_isHandlingAcceptedCandidate = true;

    if (auto range = rangeForTextCheckingResult(acceptedCandidate)) {
        if (shouldInsertText(acceptedCandidate.replacement, *range, EditorInsertAction::Typed))
            ReplaceRangeWithTextCommand::create(*range, acceptedCandidate.replacement)->apply();
    } else
        insertText(acceptedCandidate.replacement, nullptr);

    if (auto insertedCandidateRange = rangeExpandedByCharactersInDirectionAtWordBoundary(selection.visibleStart(), acceptedCandidate.replacement.length(), SelectionDirection::Backward))
        addMarker(*insertedCandidateRange, DocumentMarkerType::AcceptedCandidate, acceptedCandidate.replacement);

    m_isHandlingAcceptedCandidate = false;
}

bool Editor::unifiedTextCheckerEnabled() const
{
    return WebCore::unifiedTextCheckerEnabled(document().frame());
}

Vector<String> Editor::dictationAlternativesForMarker(const DocumentMarker& marker)
{
    return m_alternativeTextController->dictationAlternativesForMarker(marker);
}

void Editor::applyDictationAlternative(const String& alternativeString)
{
    m_alternativeTextController->applyDictationAlternative(alternativeString);
}

void Editor::toggleOverwriteModeEnabled()
{
    m_overwriteModeEnabled = !m_overwriteModeEnabled;
    document().selection().setShouldShowBlockCursor(m_overwriteModeEnabled);
}

std::optional<SimpleRange> Editor::adjustedSelectionRange()
{
    // FIXME: Why do we need to adjust the selection to include the anchor tag it's in? Whoever wrote this code originally forgot to leave us a comment explaining the rationale.
    auto range = selectedRange();
    if (range) {
        if (auto enclosingAnchor = enclosingElementWithTag(firstPositionInNode(commonInclusiveAncestor<ComposedTree>(*range)), HTMLNames::aTag)) {
            if (firstPositionInOrBeforeNode(range->start.container.ptr()) >= makeDeprecatedLegacyPosition(range->start))
                range->start = makeBoundaryPointBeforeNodeContents(*enclosingAnchor);
        }
    }
    return range;
}

// FIXME: This figures out the current style by inserting a <span>!
const RenderStyle* Editor::styleForSelectionStart(RefPtr<Node>& nodeToRemove)
{
    nodeToRemove = nullptr;

    Ref document = this->document();
    if (document->selection().isNone())
        return nullptr;

    Position position = adjustedSelectionStartForStyleComputation(document->selection().selection());
    if (!position.isCandidate() || position.isNull())
        return nullptr;

    RefPtr typingStyle { document->selection().typingStyle() };
    if (!typingStyle || !typingStyle->style())
        return &position.deprecatedNode()->renderer()->style();

    auto styleElement = HTMLSpanElement::create(document);

    auto styleText = makeAtomString(typingStyle->style()->asText(CSS::defaultSerializationContext()), " display: inline"_s);
    styleElement->setAttribute(HTMLNames::styleAttr, styleText);

    styleElement->appendChild(document->createEditingTextNode(String { emptyString() }));

    RefPtr positionNode = position.deprecatedNode();
    ASSERT(positionNode);
    RefPtr parent { positionNode->parentNode() };
    if (!parent || parent->appendChild(styleElement.get()).hasException())
        return nullptr;

    nodeToRemove = styleElement.ptr();

    document->updateStyleIfNeeded();
    return styleElement->renderer() ? &styleElement->renderer()->style() : nullptr;
}

RefPtr<Font> Editor::fontForSelection(bool& hasMultipleFonts)
{
    hasMultipleFonts = false;

    Ref document = this->document();
    if (!document->selection().isRange()) {
        RefPtr<Node> nodeToRemove;
        RefPtr<Font> font;
        {
            auto* style = styleForSelectionStart(nodeToRemove);
            if (!style)
                return nullptr;
            ScriptDisallowedScope::InMainThread scriptDisallowedScope;
            font = const_cast<Font*>(style->fontCascade().primaryFont().ptr());
        }

        if (nodeToRemove)
            nodeToRemove->remove();

        return font;
    }

    auto range = document->selection().selection().toNormalizedRange();
    if (!range)
        return nullptr;

    // FIXME: Adjusting the start may move it past the end. In that case the iterator below will go on to the end of the document.
    auto adjustedStart = makeBoundaryPoint(adjustedSelectionStartForStyleComputation(document->selection().selection()));
    if (!adjustedStart)
        return nullptr;
    range->start = *adjustedStart;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    RefPtr<Font> font;
    for (auto& node : intersectingNodes(*range)) {
        auto renderer = node.renderer();
        if (!renderer)
            continue;
        Ref primaryFont = renderer->style().fontCascade().primaryFont();
        if (!font)
            font = const_cast<Font*>(primaryFont.ptr());
        else if (font != primaryFont.ptr()) {
            hasMultipleFonts = true;
            break;
        }
    }

    return font;
}

bool Editor::canCopyExcludingStandaloneImages() const
{
    auto& selection = document().selection().selection();
    return selection.isRange() && !selection.isInPasswordField();
}

} // namespace WebCore
