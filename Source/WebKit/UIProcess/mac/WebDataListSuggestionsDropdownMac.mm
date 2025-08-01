/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebDataListSuggestionsDropdownMac.h"

#if USE(APPKIT)

#import "AppKitSPI.h"
#import "WebPageProxy.h"
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/mac/NSColorSPI.h>

constexpr CGFloat dropdownTopMargin = 3;
constexpr CGFloat dropdownVerticalPadding = 4;
constexpr CGFloat dropdownRowHeightWithoutLabel = 20;
constexpr CGFloat dropdownRowHeightWithLabel = 40;
constexpr CGFloat maximumTotalHeightForDropdownCells = 120;
static NSString * const suggestionCellReuseIdentifier = @"WKDataListSuggestionView";

@interface WKDataListSuggestionWindow : NSWindow
@end

@interface WKDataListSuggestionView : NSTableCellView
@property (nonatomic) BOOL shouldShowBottomDivider;
@end

@interface WKDataListSuggestionTableRowView : NSTableRowView
@end

@interface WKDataListSuggestionTableView : NSTableView
@end

@interface WKDataListSuggestionsController : NSObject<NSTableViewDataSource, NSTableViewDelegate>

- (id)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(NSView *)view;
- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownMac&)dropdown;
- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information;
- (void)moveSelectionByDirection:(const String&)direction;
- (void)invalidate;

- (String)currentSelectedString;

@end

namespace WebKit {

Ref<WebDataListSuggestionsDropdownMac> WebDataListSuggestionsDropdownMac::create(WebPageProxy& page, NSView *view)
{
    return adoptRef(*new WebDataListSuggestionsDropdownMac(page, view));
}

WebDataListSuggestionsDropdownMac::~WebDataListSuggestionsDropdownMac() { }

WebDataListSuggestionsDropdownMac::WebDataListSuggestionsDropdownMac(WebPageProxy& page, NSView *view)
    : WebDataListSuggestionsDropdown(page)
    , m_view(view)
{
}

void WebDataListSuggestionsDropdownMac::show(WebCore::DataListSuggestionInformation&& information)
{
    if (m_dropdownUI) {
        [m_dropdownUI updateWithInformation:WTFMove(information)];
        return;
    }

    m_dropdownUI = adoptNS([[WKDataListSuggestionsController alloc] initWithInformation:WTFMove(information) inView:m_view.get().get()]);
    [m_dropdownUI showSuggestionsDropdown:*this];
}

void WebDataListSuggestionsDropdownMac::didSelectOption(const String& selectedOption)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->didSelectOption(selectedOption);
    close();
}

void WebDataListSuggestionsDropdownMac::selectOption()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    String selectedOption = [m_dropdownUI currentSelectedString];
    if (!selectedOption.isNull())
        page->didSelectOption(selectedOption);

    close();
}

void WebDataListSuggestionsDropdownMac::handleKeydownWithIdentifier(const String& key)
{
    if (key == "Enter"_s)
        selectOption();
    else if (key == "Up"_s || key == "Down"_s)
        [m_dropdownUI moveSelectionByDirection:key];
}

void WebDataListSuggestionsDropdownMac::close()
{
    [m_dropdownUI invalidate];
    m_dropdownUI = nil;
    WebDataListSuggestionsDropdown::close();
}

} // namespace WebKit

@implementation WKDataListSuggestionWindow {
    RetainPtr<NSView> _backdropView;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingStoreType defer:(BOOL)defer
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingStoreType defer:defer];
    if (!self)
        return nil;

    self.hasShadow = YES;
#if HAVE(LIQUID_GLASS)
    if (WebKit::isLiquidGlassEnabled())
        _backdropView = adoptNS([[NSGlassEffectView alloc] initWithFrame:contentRect]);
#endif

    if (!_backdropView) {
        RetainPtr visualEffectView = adoptNS([[NSVisualEffectView alloc] initWithFrame:contentRect]);
        [visualEffectView setMaterial:NSVisualEffectMaterialMenu];
        [visualEffectView setState:NSVisualEffectStateActive];
        [visualEffectView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
        _backdropView = visualEffectView;
    }

    [_backdropView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setContentView:_backdropView.get()];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)hasKeyAppearance
{
    return YES;
}

- (NSWindowShadowOptions)shadowOptions
{
    return NSWindowShadowSecondaryWindow;
}

@end

@implementation WKDataListSuggestionView {
    RetainPtr<NSTextField> _valueField;
    RetainPtr<NSTextField> _labelField;
    RetainPtr<NSView> _bottomDivider;
}

- (id)initWithFrame:(NSRect)frameRect
{
    if (!(self = [super initWithFrame:frameRect]))
        return self;

    _valueField = adoptNS([[NSTextField alloc] init]);
    _labelField = adoptNS([[NSTextField alloc] init]);
    _bottomDivider = adoptNS([[NSView alloc] init]);
    [_bottomDivider setWantsLayer:YES];
    [_bottomDivider setHidden:YES];
    [_bottomDivider layer].backgroundColor = NSColor.separatorColor.CGColor;
    [self addSubview:_bottomDivider.get()];

    auto setUpTextField = [strongSelf = retainPtr(self)](NSTextField *textField) {
        textField.editable = NO;
        textField.bezeled = NO;
        textField.font = [NSFont menuFontOfSize:0];
        textField.drawsBackground = NO;
        [strongSelf addSubview:textField];
    };

    setUpTextField(_valueField.get());
    setUpTextField(_labelField.get());
    self.identifier = suggestionCellReuseIdentifier;

    return self;
}

- (void)layout
{
    [super layout];

    auto bounds = self.bounds;
    auto width = bounds.size.width;
    if (![_labelField isHidden]) {
        auto halfOfHeight = bounds.size.height / 2;
        [_valueField setFrame:NSMakeRect(0, halfOfHeight, width, halfOfHeight)];
        [_labelField setFrame:NSMakeRect(0, 0, width, halfOfHeight)];
    } else
        [_valueField setFrame:bounds];

    if (![_bottomDivider isHidden])
        [_bottomDivider setFrame:NSMakeRect(0, 0, width, 0.5)];
}

- (void)setValue:(NSString *)value label:(NSString *)label
{
    if ([[_valueField stringValue] isEqualToString:value] && [[_labelField stringValue] isEqualToString:label])
        return;

    [_valueField setStringValue:value];
    [_labelField setStringValue:label];
    [_labelField setHidden:!label.length];
    self.needsLayout = YES;
}

- (void)setShouldShowBottomDivider:(BOOL)showBottomDivider
{
    if ([_bottomDivider isHidden] == !showBottomDivider)
        return;

    [_bottomDivider setHidden:!showBottomDivider];
    self.needsLayout = YES;
}

- (BOOL)shouldShowBottomDivider
{
    return [_bottomDivider isHidden];
}

- (void)setBackgroundStyle:(NSBackgroundStyle)backgroundStyle
{
    [super setBackgroundStyle:backgroundStyle];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_valueField setTextColor:backgroundStyle == NSBackgroundStyleLight ? NSColor.textColor : NSColor.alternateSelectedControlTextColor];
    [_labelField setTextColor:NSColor.secondaryLabelColor];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

@end

@implementation WKDataListSuggestionTableRowView

- (void)drawSelectionInRect:(NSRect)dirtyRect
{
    [self setEmphasized:YES];
    [super drawSelectionInRect:dirtyRect];
}

@end

@implementation WKDataListSuggestionTableView

- (id)initWithElementRect:(const WebCore::IntRect&)rect
{
    if (!(self = [super initWithFrame:NSMakeRect(0, 0, rect.width(), 0)]))
        return self;

    [self setHeaderView:nil];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIntercellSpacing:NSMakeSize(0, self.intercellSpacing.height)];
    [self setStyle:NSTableViewStyleFullWidth];

    auto column = adoptNS([[NSTableColumn alloc] init]);
    [column setWidth:rect.width()];
    [self addTableColumn:column.get()];

    return self;
}

- (void)reload
{
    [self reloadData];
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

@end

static BOOL shouldShowDividersBetweenCells(const Vector<WebCore::DataListSuggestion>& suggestions)
{
    return notFound != suggestions.findIf([](auto& suggestion) {
        return !suggestion.label.isEmpty();
    });
}

@implementation WKDataListSuggestionsController {
    WeakPtr<WebKit::WebDataListSuggestionsDropdownMac> _dropdown;
    Vector<WebCore::DataListSuggestion> _suggestions;
    NSView *_presentingView;

    RetainPtr<NSScrollView> _scrollView;
    RetainPtr<WKDataListSuggestionWindow> _enclosingWindow;
    RetainPtr<WKDataListSuggestionTableView> _table;
    BOOL _showDividersBetweenCells;
}

- (id)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(NSView *)presentingView
{
    if (!(self = [super init]))
        return self;

    _presentingView = presentingView;
    _suggestions = WTFMove(information.suggestions);
    _showDividersBetweenCells = shouldShowDividersBetweenCells(_suggestions);
    _table = adoptNS([[WKDataListSuggestionTableView alloc] initWithElementRect:information.elementRect]);

    _enclosingWindow = adoptNS([[WKDataListSuggestionWindow alloc] initWithContentRect:NSZeroRect styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [_enclosingWindow setReleasedWhenClosed:NO];
    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
    [_enclosingWindow setTitleVisibility:NSWindowTitleHidden];
    [_enclosingWindow setTitlebarAppearsTransparent:YES];
    [_enclosingWindow setMovable:NO];
    [_enclosingWindow setBackgroundColor:[NSColor clearColor]];
    [_enclosingWindow setOpaque:NO];

#if HAVE(LIQUID_GLASS)
    if (WebKit::isLiquidGlassEnabled()) {
        [_enclosingWindow setStyleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskFullSizeContentView)];
        [[[_enclosingWindow contentView] layer] setCornerRadius:12.0];
        [[[_enclosingWindow contentView] layer] setMasksToBounds:YES];
    }
#endif

    _scrollView = adoptNS([[NSScrollView alloc] initWithFrame:[_enclosingWindow contentView].bounds]);
    [_scrollView setHasVerticalScroller:YES];
    [_scrollView setVerticalScrollElasticity:NSScrollElasticityAllowed];
    [_scrollView setHorizontalScrollElasticity:NSScrollElasticityNone];
    [_scrollView setDocumentView:_table.get()];
    [_scrollView setDrawsBackground:NO];

    auto insetView = _scrollView;
    [insetView setAutomaticallyAdjustsContentInsets:NO];
    [insetView setContentInsets:NSEdgeInsetsMake(dropdownVerticalPadding, 0, dropdownVerticalPadding, 0)];

    [_table setDelegate:self];
    [_table setDataSource:self];
    [_table setAction:@selector(selectedRow:)];
    [_table setTarget:self];

    return self;
}

- (String)currentSelectedString
{
    NSInteger selectedRow = [_table selectedRow];

    if (selectedRow >= 0 && static_cast<size_t>(selectedRow) < _suggestions.size())
        return _suggestions.at(selectedRow).value;

    return String();
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    _suggestions = WTFMove(information.suggestions);
    _showDividersBetweenCells = shouldShowDividersBetweenCells(_suggestions);
    [_table reload];

    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
    [_scrollView setFrame:[_enclosingWindow contentView].bounds];
}

- (void)notifyAccessibilityClients:(NSString *)info
{
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, @{
        NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey: info,
    });
}

- (void)moveSelectionByDirection:(const String&)direction
{
    size_t size = _suggestions.size();
    NSInteger oldSelection = [_table selectedRow];
    NSInteger newSelection = -1;

    if (oldSelection == -1)
        newSelection = (direction == "Up"_s) ? (size - 1) : 0;
    else {
        NSInteger adjustment = (direction == "Up"_s) ? -1 : 1;
        newSelection = std::clamp<NSInteger>(oldSelection + adjustment, 0, size);
    }

    if (oldSelection == newSelection)
        return;

    [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:newSelection] byExtendingSelection:NO];
    [_table scrollRowToVisible:newSelection];

    // Notify accessibility clients of new selection.
    RetainPtr currentSelectedString = [self currentSelectedString].createNSString();
    [self notifyAccessibilityClients:currentSelectedString.get()];
}

- (void)invalidate
{
    [_table removeFromSuperviewWithoutNeedingDisplay];
    [_scrollView removeFromSuperviewWithoutNeedingDisplay];

    [_table setDelegate:nil];
    [_table setDataSource:nil];
    [_table setTarget:nil];

    _table = nil;
    _scrollView = nil;

    [[_presentingView window] removeChildWindow:_enclosingWindow.get()];
    [_enclosingWindow close];
    _enclosingWindow = nil;

    // Notify accessibility clients that datalist went away.
    RetainPtr info = WEB_UI_STRING("Suggestions list hidden.", "Accessibility announcement for the data list suggestions dropdown going away.").createNSString();
    [self notifyAccessibilityClients:info.get()];
}

- (NSRect)dropdownRectForElementRect:(const WebCore::IntRect&)rect
{
    RetainPtr presentingWindow = [_presentingView window];
    NSRect screenRect = presentingWindow.get().screen.visibleFrame;
    NSRect windowRect = [presentingWindow convertRectToScreen:[_presentingView convertRect:rect toView:nil]];

    windowRect = CGRectIntersection(windowRect, screenRect);
    if (CGRectIsNull(windowRect))
        return NSZeroRect;

    CGFloat totalCellHeight = 0;
    for (auto& suggestion : _suggestions)
        totalCellHeight += suggestion.label.isEmpty() ? dropdownRowHeightWithoutLabel : dropdownRowHeightWithLabel;

    CGFloat totalIntercellSpacingAndPadding = dropdownVerticalPadding * 2;
    if (_suggestions.size() > 1)
        totalIntercellSpacingAndPadding += (_suggestions.size() - 1) * [_table intercellSpacing].height;

    CGFloat width = std::min<CGFloat>(std::max(rect.width(), rect.height()), screenRect.size.width);
    CGFloat height = std::min<CGFloat>(totalIntercellSpacingAndPadding + std::min(totalCellHeight, maximumTotalHeightForDropdownCells), screenRect.size.height);
    CGFloat originX = std::max<CGFloat>(NSMinX(windowRect), 0);
    CGFloat originY = std::max<CGFloat>(NSMinY(windowRect) - height - dropdownTopMargin, 0);

    return NSMakeRect(originX, originY, width, height);
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownMac&)dropdown
{
    _dropdown = dropdown;
    [[_enclosingWindow contentView] addSubview:_scrollView.get()];
    [_table reload];
    [[_presentingView window] addChildWindow:_enclosingWindow.get() ordered:NSWindowAbove];
    [_scrollView flashScrollers];

    // Notify accessibility clients of datalist becoming visible.
    RetainPtr currentSelectedString = [self currentSelectedString].createNSString();
    RetainPtr info = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Suggestions list visible, %@", "Accessibility announcement that the suggestions list became visible. The format argument is for the first option in the list."), currentSelectedString.get()]);
    [self notifyAccessibilityClients:info.get()];
}

- (void)selectedRow:(NSTableView *)sender
{
    auto selectedString = self.currentSelectedString;
    if (!selectedString)
        return;

    Ref { *_dropdown }->didSelectOption(selectedString);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _suggestions.size();
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    auto suggestionIndex = static_cast<size_t>(row);
    if (suggestionIndex >= _suggestions.size()) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    return _suggestions.at(suggestionIndex).label.isEmpty() ? dropdownRowHeightWithoutLabel : dropdownRowHeightWithLabel;
}

- (NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row
{
    return adoptNS([[WKDataListSuggestionTableRowView alloc] init]).autorelease();
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    auto result = retainPtr([tableView makeViewWithIdentifier:suggestionCellReuseIdentifier owner:self]);

    if (!result) {
        result = adoptNS([[WKDataListSuggestionView alloc] init]);
        [result setIdentifier:suggestionCellReuseIdentifier];
    }

    auto& suggestion = _suggestions.at(row);
    [result setShouldShowBottomDivider:_showDividersBetweenCells && row < static_cast<NSInteger>(_suggestions.size() - 1)];
    [result setValue:suggestion.value.createNSString().get() label:suggestion.label.createNSString().get()];

    return result.autorelease();
}

@end

#endif // USE(APPKIT)
