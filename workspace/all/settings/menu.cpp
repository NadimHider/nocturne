#include "menu.hpp"

extern "C"
{
#include "defines.h"
#include "api.h"
#include "utils.h"
}

#include <cmath>
#include <mutex>
#include <shared_mutex>
typedef std::shared_mutex Lock;
typedef std::unique_lock< Lock >  WriteLock;
typedef std::shared_lock< Lock >  ReadLock;

static std::string overlayMessage;
static bool overlayVisible = false;
static OverlayDismissMode overlayDismissMode = OverlayDismissMode::None;
static SDL_Surface* overlaySurface = nullptr;
static Lock overlayLock;

static void drawOverlayLocal(SDL_Surface* screen);

///////////////////////////////////////////////////////////

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
                   const std::vector<std::any> &values, const std::vector<std::string> &labels,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset, 
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu), 
    values(values), labels(labels)
{
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, const std::vector<std::any> &values,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu), 
    values(values) /*labels({}),*/
{
    generateDefaultLabels();
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, 
                   int min, int max, const std::string suffix,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu)
{
    const int step = 1; // until we need it
    const int num = (max - min) / step + 1;
    for (int i = 0; i < num; i++)
        values.push_back(min + i * step);

    generateDefaultLabels(suffix);
    initSelection();
    assert(valueIdx >= 0);
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
    MenuListCallback on_confirm, MenuList *submenu)
    : MenuItem(type, name, desc, 0,0, "", nullptr, nullptr, nullptr, on_confirm, submenu)
{}

void MenuItem::generateDefaultLabels(const std::string& suffix)
{
    labels.clear();
    for (auto v : values)
    {
        if (v.type() == typeid(std::string))
            labels.push_back(std::any_cast<std::string>(v) + suffix);
        else if (v.type() == typeid(float))
            labels.push_back(std::to_string(std::any_cast<float>(v)) + suffix);
        else if (v.type() == typeid(int))
            labels.push_back(std::to_string(std::any_cast<int>(v)) + suffix);
        else if (v.type() == typeid(uint32_t))
            labels.push_back(std::to_string(std::any_cast<uint32_t>(v)) + suffix);
        else if (v.type() == typeid(bool))
            labels.push_back((std::any_cast<bool>(v) ? "On" : "Off") + suffix);
        else
            assert(false); // needs more string conversion
    }
}

void MenuItem::initSelection()
{
    int oldValueIdx = valueIdx;
    valueIdx = -1;
    if (!values.empty())
    {
        valueIdx = 0;
        if (on_get)
        {
            // we know we can convert both std::any values to the same type
            const auto initialVal = on_get();
            try
            {
                for (int i = 0; i < values.size(); i++)
                {
                    const auto &v = values[i];
                    if (v.type() != initialVal.type())
                        LOG_error("type mismatch: %s vs. %s", v.type().name(), initialVal.type().name());

                    assert(v.type() == initialVal.type());

                    if (v.type() == typeid(float))
                    {
                        if (std::any_cast<float>(initialVal) == std::any_cast<float>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(int))
                    {
                        if (std::any_cast<int>(initialVal) == std::any_cast<int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(unsigned int))
                    {
                        if (std::any_cast<unsigned int>(initialVal) == std::any_cast<unsigned int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(uint32_t))
                    {
                        if (std::any_cast<uint32_t>(initialVal) == std::any_cast<uint32_t>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(bool))
                    {
                        if (std::any_cast<bool>(initialVal) == std::any_cast<bool>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::string))
                    {
                        if (std::any_cast<std::string>(initialVal) == std::any_cast<std::string>(v))
                        {
                            //LOG_info("Found equal strings %s and %s\n", initialVal, v);
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::basic_string<char>))
                    {
                        if (std::any_cast<std::basic_string<char>>(initialVal) == std::any_cast<std::basic_string<char>>(v))
                        {
                            //LOG_info("Found equal basic strings %s and %s\n", initialVal, v);
                            valueIdx = i;
                            break;
                        }
                    }
                    else
                    {
                        LOG_warn("Cant initialize selection for %s from unknown type %s\n", this->getLabel(), v.type().name());
                        // assert(false);
                    }
                }
            }
            catch (...)
            {
                // bad any cast
                LOG_warn("Bad any cast for %s\n", this->getLabel());
            }
            // this sadly doesnt work with std::any
            // auto it = std::find(values.cbegin(), values.cend(), on_get());
            // if (it == std::end(values))
            //    valueIdx = -1;
            // else
            //    valueIdx = std::distance(values.cbegin(), it);
        }
        assert(valueIdx >= 0);
    }
}

InputReactionHint MenuItem::handleInput(int &dirty)
{
    InputReactionHint hint = Unhandled;

    if (deferred)
    {
        assert(submenu);
        int subMenuJustClosed = 0;
        hint = submenu->handleInput(dirty, subMenuJustClosed);
        if (subMenuJustClosed) {
            defer(false);
            dirty = 1;
        }
        return hint;
    }

    // handle our custom behavior and return true if the input was handled
    if (PAD_justRepeated(BTN_LEFT))
    {
        hint = NoOp;
        if (prevValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_RIGHT))
    {
        hint = NoOp;
        if (nextValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    if (PAD_justRepeated(BTN_L1))
    {
        hint = NoOp;
        if (prev(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_R1))
    {
        hint = NoOp;
        if (next(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justPressed(BTN_A))
    {
        hint = NoOp; // not really, should check on_confirm
        if (on_confirm)
            hint = on_confirm(*this);
        dirty = 1;
    }

    return hint;
}

bool MenuItem::next(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + n) % values.size();
    return true;
}

bool MenuItem::prev(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + values.size() - n) % values.size();
    return true;
}

///////////////////////////////////////////////////////////

InputReactionHint TextInputMenuItem::handleInput(int &dirty) {
    // Handle deferred state - route input to keyboard submenu
    if (deferred) {
        assert(submenu);
        int subMenuJustClosed = 0;
        auto hint = submenu->handleInput(dirty, subMenuJustClosed);
        if (subMenuJustClosed) {
            defer(false);
            dirty = 1;
            // Don't propagate Exit up - just stay on current menu
            return NoOp;
        }
        return hint;
    }

    // Open keyboard when A is pressed
    if (PAD_justPressed(BTN_A)) {
        if (on_confirm) {
            auto hint = on_confirm(*this);
            dirty = 1;
            return hint;
        }
    }
    return Unhandled;
}

///////////////////////////////////////////////////////////

MenuList::MenuList(MenuItemType type, const std::string &descp, std::vector<AbstractMenuItem*> items, MenuListCallback on_change, MenuListCallback on_confirm)
    : type(type), desc(descp), items(items), on_change(on_change), on_confirm(on_confirm)
{
    // best effort layout based on the platform defines, user should really call performLayout manually
    performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
    layout_called = false;
}

MenuList::~MenuList()
{
    WriteLock w(itemLock);
    for (auto item : items)
        delete item;
    items.clear();
}

void MenuList::performLayout(const SDL_Rect &dst)
{
    ReadLock r(itemLock);
    scope.count = items.size();
    scope.selected = std::clamp(scope.selected, 0, std::max(0, scope.count - 1));
    layout_rect = dst;
    layout_row_height = GFX_getRowHeight(font.small, BUTTON_SIZE);
    max_width = 0;
    if (type == MenuItemType::Main)
    {
        scope.max_visible_options = std::max(1, (dst.h - SCALE1(PILL_SIZE)) / GFX_getRowHeight(font.large, PILL_SIZE));
    }
    else
    {
        const int description_height = std::max(SCALE1(PILL_SIZE), 2 * TTF_FontLineSkip(font.tiny) + SCALE1(6));
        scope.max_visible_options = std::max(1, (dst.h - description_height) / layout_row_height);
    }
    scope.visible_rows = std::min(scope.count, scope.max_visible_options);
    scope.start = std::clamp(scope.start, 0, std::max(0, scope.count - scope.visible_rows));
    if (scope.selected < scope.start) scope.start = scope.selected;
    if (scope.selected >= scope.start + scope.visible_rows)
        scope.start = std::max(0, scope.selected - scope.visible_rows + 1);
    scope.end = std::min(scope.count, scope.start + scope.visible_rows);

    for (auto itm : items)
        if (itm->getSubMenu())
            itm->getSubMenu()->performLayout(dst);

    layout_called = true;
}

bool MenuList::selectNext()
{
    if (!scope.count) return false;
    scope.selected++;
    if (scope.selected >= scope.count)
    {
        scope.selected = 0;
        scope.start = 0;
        scope.end = scope.visible_rows;
    }
    else if (scope.selected >= scope.end)
    {
        scope.start++;
        scope.end++;
    }

    return true;
}

bool MenuList::selectPrev()
{
    if (!scope.count) return false;
    scope.selected--;
    if (scope.selected < 0)
    {
        scope.selected = scope.count - 1;
        scope.start = std::max(0, scope.count - scope.max_visible_options);
        scope.end = scope.count;
    }
    else if (scope.selected < scope.start)
    {
        scope.start--;
        scope.end--;
    }

    return true;
}

std::string MenuList::getSelectedItemName() const
{
    if(items.empty())
        return "";

    return items.at(scope.selected)->getName();
}

bool MenuList::selectByName(const std::string &name)
{
    if(name.empty())
        return false;

    int toSelect = -1;
    for (int i = 0; i < items.size() && toSelect < 0; i++)
        if(items.at(i)->getName() == name)
            toSelect = i;
    //LOG_info("Found element %s (%d) at pos %d\n", name.c_str(), scope.selected - scope.start, toSelect);
    if (toSelect >= 0)
    {
        scope.selected = toSelect;
        performLayout(layout_rect);
        return true;
    }
    return false;
}

// returns true if the input was handled
InputReactionHint MenuList::handleInput(int &dirty, int &quit)
{
    bool tryDismiss = false;
    {
        ReadLock r(overlayLock);
        if (overlayVisible) {
             bool dismissed = false;
             if (overlayDismissMode == OverlayDismissMode::DismissOnA && PAD_justPressed(BTN_A)) dismissed = true;
             else if (overlayDismissMode == OverlayDismissMode::DismissOnB && PAD_justPressed(BTN_B)) dismissed = true;

             if (dismissed) {
                 tryDismiss = true;
             } else {
                 return NoOp;
             }
        }
    }

    if (tryDismiss) {
        hideOverlay();
        dirty = 1;
        return NoOp;
    }

    ReadLock r(itemLock);
    if (items.empty()) {
        if (PAD_justPressed(BTN_B)) quit = 1;
        return NoOp;
    }
    InputReactionHint handled = items.at(scope.selected)->handleInput(dirty);
    if(handled == ResetAllItems) {
        resetAllItems();
        dirty = 1;
        return NoOp;
    }
    else if (handled == Exit) {
        quit = 1;
        return NoOp;
    }
    else if (handled != Unhandled)
        return handled;

    if (PAD_justRepeated(BTN_UP))
    {
        if (scope.selected == 0 && !PAD_justPressed(BTN_UP))
            return NoOp; // stop at top
        if (selectPrev())
            dirty = 1;
        return NoOp;
    }
    else if (PAD_justRepeated(BTN_DOWN))
    {
        if (scope.selected == scope.count - 1 && !PAD_justPressed(BTN_DOWN))
            return NoOp; // stop at bottom
        if (selectNext())
            dirty = 1;
        return NoOp;
    }
    else if (on_change)
    {
        // do we even need this?
    }
    else if (on_confirm && PAD_justPressed(BTN_A))
    {
        // do we even need this?
    }
    else if (PAD_justPressed(BTN_B))
    {
        quit = 1;
        return NoOp;
    }

    return Unhandled;
}

SDL_Rect MenuList::itemSizeHint(const AbstractMenuItem &item)
{
    if (type == MenuItemType::Fixed)
    {
        return {0, 0, 0, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::List)
    {
        // calculate the size of the list
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Input || type == MenuItemType::Var)
    {
        int w = 0;
        int lw = 0;
        int rw = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &lw, NULL);
        // get the width of the widest row
        int mrw = 0;
        // every value list in an input table is the same
        // so only calculate rw for the first item...
        if (!mrw || type != MenuItemType::Input)
        {
            for (int j = 0; item.getValues().size() > j && !item.getLabels()[j].empty(); j++)
            {
                TTF_SizeUTF8(font.small, item.getLabels()[j].c_str(), &rw, NULL);
                if (lw + rw > w)
                    w = lw + rw;
                if (rw > mrw)
                    mrw = rw;
            }
        }
        else
        {
            w = lw + mrw;
        }
        w += SCALE1(OPTION_PADDING * 4);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Main)
    {
        int w = 0;
        TTF_SizeUTF8(font.large, item.getName().c_str(), &w, NULL);
        w += SCALE1(BUTTON_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else
    {
        assert(false); // new enum value?
    }
}

void MenuList::draw(SDL_Surface *surface, const SDL_Rect &dst, const SDL_Rect &dstTitle)
{
    assert(layout_called);
    if (layout_row_height != GFX_getRowHeight(font.small, BUTTON_SIZE) ||
        layout_rect.x != dst.x || layout_rect.y != dst.y || layout_rect.w != dst.w || layout_rect.h != dst.h)
        performLayout(dst);
    // Font/style changes can alter widths even when row height is unchanged.
    max_width = 0;
    ReadLock r(itemLock);

    auto cur = !items.empty() ? items.at(scope.selected) : nullptr;
    if (cur && cur->isDeferred())
    {
        assert(cur->getSubMenu());
        cur->getSubMenu()->draw(surface, dst, dstTitle);
    }
    else
    {
        // iterate all items and draw them vertically
        switch (type)
        {
        case MenuItemType::List:
            drawList(surface, dst, dstTitle);
            break;
        case MenuItemType::Fixed:
            drawFixed(surface, dst, dstTitle);
            break;
        case MenuItemType::Var:
        case MenuItemType::Input:
            drawInput(surface, dst, dstTitle);
            break;
        case MenuItemType::Main:
            drawMain(surface, dst, dstTitle);
            break;
        case MenuItemType::Custom:
            drawCustom(surface, dst, dstTitle);
            return; // no further drawing over custom
        default:
            assert(false && "Unknown list type");
        }

        // Handle overflow (anything but Main and Custom)
        if (type != MenuItemType::Main && items.size() > scope.max_visible_options)
        {
            const int SCROLL_WIDTH = 24;
            const int SCROLL_HEIGHT = 6;
            SDL_Rect rect = dst;
            rect = dx(rect, (rect.w - SCALE1(SCROLL_WIDTH)) / 2);
            rect = dy(rect, SCALE1((-SCROLL_HEIGHT) / 2));
            rect.w = SCALE1(SCROLL_WIDTH);
            rect.h = scope.visible_rows * layout_row_height;

            if (scope.start > 0)
                // assumes there is some padding above we can yoink
                GFX_blitAssetCPP(ASSET_SCROLL_UP, {}, surface, {rect.x, rect.y - SCALE1(PADDING)});
            if (scope.end < scope.count)
                GFX_blitAssetCPP(ASSET_SCROLL_DOWN, {}, surface, {rect.x, rect.y + rect.h + (SCALE1(SCROLL_HEIGHT) / 2)});
        }

        if (cur && cur->getDesc().length() > 0)
        {
            const auto description = cur->getDesc();
            const int leading = TTF_FontLineSkip(font.tiny);
            GFX_blitWrappedText(font.tiny, description.c_str(), dst.w, 2,
                uintToColour(THEME_COLOR6_255), surface, dst.y + dst.h - 2 * leading);
        }
    }

    {
        WriteLock w(overlayLock);
        overlaySurface = surface;
    }

    drawOverlayLocal(surface);
}

void MenuList::drawList(SDL_Surface *surface, const SDL_Rect &dst, const SDL_Rect &dstTitle)
{
    // we ignore type here, it all paints the same.
    if (max_width == 0)
    {
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        // cache the result
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, j * layout_row_height);
        pos.h = layout_row_height;
        drawListItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawListItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    std::vector<char> fitted(item.getName().size() + 1);
    int width = GFX_truncateText(font.small, item.getName().c_str(), fitted.data(), fitted.size(),
        std::max(0, dst.w - SCALE1(OPTION_PADDING * 2)), 0);
    if (selected)
    {
        // move out of conditional if centering
        int w = width + SCALE1(OPTION_PADDING * 2);

        GFX_blitPillDarkCPP(ASSET_BUTTON, surface, {dst.x, dst.y + SCALE1(ROW_PILL_INSET), w, dst.h - SCALE1(ROW_PILL_INSET * 2)});
        text_color = uintToColour(THEME_COLOR5_255);
    }
    text = TTF_RenderUTF8_Blended(font.small, fitted.data(), text_color);
    if (text) {
        SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + ((dst.h - text->h) / 2)});
        SDL_FreeSurface(text);
    }
}

void MenuList::drawFixed(SDL_Surface *surface, const SDL_Rect &dst, const SDL_Rect &dstTitle)
{
    // NOTE: no need to calculate max width
    int mw = dst.w;
    // int lw,rw;
    // lw = rw = mw / 2;
    max_width = mw; // not sure about this one

    SDL_Rect rect = dst;

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, j * layout_row_height);
        pos.h = layout_row_height;
        drawFixedItem(surface, pos, *items[i], j == selected_row);
    }
}

// TODO: expose API functions that do the same
namespace
{
    static inline void rgba_unpack(uint32_t col, int *r, int *g, int *b, int *a)
    {
        *r = (col >> 24) & 0xff;
        *g = (col >> 16) & 0xff;
        *b = (col >> 8) & 0xff;
        *a = col & 0xff;
    }

    static inline uint32_t mapUint(SDL_Surface *surface, uint32_t col)
    {
        int r, g, b, a;
        rgba_unpack(col, &r, &g, &b, &a);
        return SDL_MapRGBA(surface->format, r, g, b, a);
    }
}

void MenuList::drawFixedItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    const int padding = SCALE1(OPTION_PADDING);
    const int inset = SCALE1(ROW_PILL_INSET);
    const int inner_width = std::max(0, dst.w - padding * 2);
    const bool custom = item.getType() == ListItemType::Custom;
    const bool color = item.getType() == ListItemType::Color;
    const bool has_value = item.getValue().has_value() && item.getType() != ListItemType::Button;
    const int swatch_size = color ? TTF_FontHeight(font.small) / 2 : 0;
    const int swatch_space = color ? swatch_size + SCALE1(6) : 0;
    std::string value = has_value && !custom ? item.getLabel() : "";
    uint32_t raw_color = 0;
    if (color && has_value) {
        raw_color = item.on_get ? std::any_cast<uint32_t>(item.on_get()) : std::any_cast<uint32_t>(item.getValue());
        char hex[12];
        snprintf(hex, sizeof(hex), "0x%08X", raw_color);
        value = hex;
    }
    std::vector<char> fitted_value(value.size() + 1);
    GFX_truncateText(font.small, value.c_str(), fitted_value.data(), fitted_value.size(),
        std::max(0, inner_width / 2 - swatch_space), 0);
    int value_width = 0;
    TTF_SizeUTF8(font.small, fitted_value.data(), &value_width, nullptr);
    const int reserved = custom ? inner_width / 3 :
        (has_value ? value_width + swatch_space + padding : 0);
    std::vector<char> fitted_name(item.getName().size() + 1);
    const int name_width = GFX_truncateText(font.small, item.getName().c_str(), fitted_name.data(), fitted_name.size(),
        std::max(0, inner_width - reserved), 0);

    GFX_blitPillColorCPP(ASSET_BUTTON, surface,
        {dst.x, dst.y + inset, dst.w, dst.h - inset * 2},
        selected ? THEME_COLOR2 : THEME_COLOR3, RGB_WHITE);
    if (custom) item.drawCustomItem(surface, dst, item, selected);
    if (selected)
        GFX_blitPillDarkCPP(ASSET_BUTTON, surface,
            {dst.x, dst.y + inset, std::min(dst.w, name_width + padding * 2), dst.h - inset * 2});
    SDL_Surface *text = TTF_RenderUTF8_Blended(font.small, fitted_name.data(),
        uintToColour(selected ? THEME_COLOR5_255 : THEME_COLOR4_255));
    if (text) {
        SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + padding, dst.y + (dst.h - text->h) / 2});
        SDL_FreeSurface(text);
    }
    text = TTF_RenderUTF8_Blended(font.small, fitted_value.data(), uintToColour(THEME_COLOR4_255));
    if (text) {
        SDL_BlitSurfaceCPP(text, {}, surface,
            {dst.x + dst.w - padding - swatch_space - text->w, dst.y + (dst.h - text->h) / 2});
        SDL_FreeSurface(text);
    }
    if (color && has_value) {
        SDL_Rect swatch = {dst.x + dst.w - padding - swatch_size, dst.y + (dst.h - swatch_size) / 2,
            swatch_size, swatch_size};
        SDL_FillRect(surface, &swatch, THEME_COLOR6);
        swatch.x++; swatch.y++; swatch.w -= 2; swatch.h -= 2;
        SDL_FillRect(surface, &swatch, mapUint(surface, raw_color));
    }
}

void MenuList::drawInput(SDL_Surface *surface, const SDL_Rect &dst, const SDL_Rect &dstTitle)
{
    // TODO: handle type if we need it
    if (max_width == 0)
    {
        // get the width of the widest row
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        // cache the result
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, j * layout_row_height);
        pos.h = layout_row_height;
        pos.w = max_width;
        drawInputItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawInputItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    drawFixedItem(surface, dst, item, selected);
}

void MenuList::drawMain(SDL_Surface *surface, const SDL_Rect &dst, const SDL_Rect &dstTitle)
{
    // we ignore type here, it all paints the same.
    // no size calc to do here, each line is as wide as needed.

    if (scope.count > 0)
    {
        int selected_row = scope.selected - scope.start;
        for (int i = scope.start, j = 0; i < scope.end; i++, j++)
        {
            auto pos = dy(dst, j * GFX_getRowHeight(font.large, PILL_SIZE));
            pos.h = GFX_getRowHeight(font.large, PILL_SIZE);
            // auto hintRect = itemSizeHint(items[i]);
            // pos.w = std::min(pos.w, hintRect.w);
            // pos.h = std::min(pos.h, hintRect.h);

            drawMainItem(surface, pos, *items[i], j == selected_row);
        }
    }
    else
    {
        GFX_blitMessageCPP(font.large, "Empty folder", surface, dst);
    }
}

void MenuList::drawMainItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    // TODO: unique item handling (draws grey text)
    const bool unique = false;

    char truncated[256];
    int text_width = GFX_truncateText(font.large, item.getName().c_str(), truncated, sizeof(truncated), dst.w, SCALE1(BUTTON_PADDING * 2));
    int max_width = std::min(dst.w, text_width);

    if (selected)
    {
        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, max_width, dst.h});
        text_color = uintToColour(THEME_COLOR5_255);
    }
    else if (unique)
    {
        // TODO: port this over when needed. Its complete spaghetti code...
    }
    text = TTF_RenderUTF8_Blended(font.large, truncated, text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(BUTTON_PADDING), dst.y + ((dst.h - text->h) / 2)});
    SDL_FreeSurface(text);
}

void MenuList::resetAllItems()
{
    ReadLock r(itemLock);
    for(auto item : items) {
        if(item->on_reset) {
            item->on_reset();
            item->initSelection();
 
        }
    }
}

void MenuList::showOverlay(const std::string& message, OverlayDismissMode dismissMode)
{
    {
        WriteLock w(overlayLock);
        overlayMessage = message;
        overlayVisible = true;
        overlayDismissMode = dismissMode;
    }
    
    // We want to force a draw right now since usually we are about to block
    WriteLock w(overlayLock);
    if (overlaySurface) {
        // Clear the surface first to prevent text ghosting from previous
        // overlay frames (the semi-transparent shadow doesn't fully obscure
        // old content, causing overlap when updated repeatedly in a loop)
        SDL_FillRect(overlaySurface, NULL, SDL_MapRGB(overlaySurface->format, 0, 0, 0));
        drawOverlayLocal(overlaySurface);
        GFX_flip(overlaySurface);
    }
}

void MenuList::hideOverlay()
{
    WriteLock w(overlayLock);
    overlayVisible = false;
}

bool MenuList::isOverlayVisible()
{
    ReadLock r(overlayLock);
    return overlayVisible;
}


static void drawOverlayLocal(SDL_Surface* screen) {
    // ReadLock r(overlayLock); // Assumes caller held lock or is safe
    if (!overlayVisible) return;
    
    if (!screen) return;

    // Use a static shadow surface to create a semi-transparent overlay
    static SDL_Surface* shadow = nullptr;
    if (!shadow || shadow->w != screen->w || shadow->h != screen->h) {
        if (shadow) SDL_FreeSurface(shadow);
        shadow = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h, 
                                      screen->format->BitsPerPixel, 
                                      screen->format->Rmask, screen->format->Gmask, screen->format->Bmask, 0);
        SDL_FillRect(shadow, NULL, SDL_MapRGB(shadow->format, 0, 0, 0));
        SDLX_SetAlpha(shadow, SDL_SRCALPHA, 200); // Semi-transparent black
    }
    SDL_BlitSurface(shadow, NULL, screen, NULL);

    SDL_Rect screenRect = {0, 0, screen->w, screen->h};

    GFX_blitMessageCPP(font.medium, overlayMessage, screen, screenRect);
    
    if (overlayDismissMode != OverlayDismissMode::None) {
        if (overlayDismissMode == OverlayDismissMode::DismissOnB) {
            char *hints[] = {(char *)("B"), (char *)("BACK"), NULL};
            GFX_blitButtonGroup(hints, 1, screen, 1);
        } else if (overlayDismissMode == OverlayDismissMode::DismissOnA) {
            char *hints[] = {(char *)("A"), (char *)("OK"), NULL};
            GFX_blitButtonGroup(hints, 1, screen, 1);
        }
    }
}
