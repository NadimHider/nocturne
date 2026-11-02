extern "C"
{
#include "msettings.h"

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "displaycal.h"
#include "ra_auth.h"
#include "ra_sync.h"
}

#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <regex>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include "wifimenu.hpp"
#include "btmenu.hpp"
#include "keyboardprompt.hpp"
#include "colorpickermenu.hpp"
#include "palettemenu.hpp"
#include "fnbuttonmenu.hpp"

#define BUSYBOX_STOCK_VERSION "1.27.2"

static int appQuit = false;
static bool appSuspend = false;

static void sigHandler(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        appQuit = true;
        break;
    case SIGSTOP:
        appSuspend = true;
        break;
    case SIGCONT:
        appSuspend = false;
        break;
    default:
        break;
    }
}

struct Context
{
    MenuList *menu;
    SDL_Surface *screen;
    int dirty;
    int show_setting;

    // Either the app manages these, or we account for the space and let the menu draw it
    // We could hardcode the behavior down below, but this should also serve as demo code
    // for how to use menu.cpp in different ways depending on the needs of the app
    bool appManagesTitle = false;
    bool appManagesIndicator = true;
    bool appManagesHints = false;
};

// This is all the MinUiSettings stuff, for now just copied over from the old settings app

static const std::vector<std::any> colors = {
    0x8BD3C7U, 0xD8B487U, 0xB9A7E6U, 0x8BBED3U, 0xA8C7A2U, 0xD49A9AU, 0xE4EBEFU, 0xA7B6C0U, 0x6F8794U, 0x3E535EU, 0x24313AU, 0x12191FU, 0x070A0DU, 0xD4B483U, 0x332A23U, 0x1B1714U, 0xB4AAC5U, 0x2D273DU, 0x191622U, 0xA3B8C4U, 0x22333EU, 0x111A20U, 0x000000U, 0xFFFFFFU};
static const std::vector<std::string> color_strings = {
    "0x8BD3C7", "0xD8B487", "0xB9A7E6", "0x8BBED3", "0xA8C7A2", "0xD49A9A", "0xE4EBEF", "0xA7B6C0", "0x6F8794", "0x3E535E", "0x24313A", "0x12191F", "0x070A0D", "0xD4B483", "0x332A23", "0x1B1714", "0xB4AAC5", "0x2D273D", "0x191622", "0xA3B8C4", "0x22333E", "0x111A20", "0x000000", "0xFFFFFF"};

struct FontEntry {
    std::string filename;
    std::string label;
};

static std::vector<FontEntry> enumerateFonts() {
    std::vector<FontEntry> fonts;
    std::vector<FontEntry> custom_fonts;

    // Featured font at top
    fonts.push_back({"DepartureMono-Regular.otf", "Departure Mono"});

    DIR *dir = opendir(RES_PATH);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            // skip the featured font already added
            if (strcmp(ent->d_name, "DepartureMono-Regular.otf") == 0)
                continue;
            size_t len = strlen(ent->d_name);
            if (len < 5) continue;
            const char *ext = ent->d_name + len - 4;
            if (strcasecmp(ext, ".ttf") != 0 && strcasecmp(ext, ".otf") != 0)
                continue;

            // build clean display name: strip extension, remove -Regular
            std::string label(ent->d_name, len - 4);
            size_t reg_pos = label.find("-Regular");
            if (reg_pos != std::string::npos) label.erase(reg_pos, 8);
            std::replace(label.begin(), label.end(), '_', ' ');

            custom_fonts.push_back({std::string(ent->d_name), label});
        }
        closedir(dir);
    }

    std::sort(custom_fonts.begin(), custom_fonts.end(), [](const FontEntry &a, const FontEntry &b) {
        return a.label < b.label;
    });

    for (const auto &cf : custom_fonts) {
        fonts.push_back(cf);
    }

    return fonts;
}

static const std::vector<std::any>    screen_timeout_secs = {0U, 5U, 10U, 15U, 30U, 45U, 60U, 90U, 120U, 240U, 360U, 600U};
static const std::vector<std::string> screen_timeout_labels = {"Never", "5s", "10s", "15s", "30s", "45s", "60s", "90s", "2m", "4m", "6m", "10m"};

static const std::vector<std::any>    sleep_timeout_secs = {5U, 10U, 15U, 30U, 45U, 60U, 90U, 120U, 240U, 360U, 600U};
static const std::vector<std::string> sleep_timeout_labels = {"5s", "10s", "15s", "30s", "45s", "60s", "90s", "2m", "4m", "6m", "10m"};

static const std::vector<std::string> on_off = {"Off", "On"};

static const std::vector<std::string> scaling_strings = {"Fullscreen", "Fit", "Fill"};
static const std::vector<std::any> scaling = {(int)GFX_SCALE_FULLSCREEN, (int)GFX_SCALE_FIT, (int)GFX_SCALE_FILL};

// Notification duration options (in seconds)
static const std::vector<std::any> notify_duration_values = {1, 2, 3, 4, 5};
static const std::vector<std::string> notify_duration_labels = {"1s", "2s", "3s", "4s", "5s"};

// Progress notification duration options (in seconds, 0 = disabled)
static const std::vector<std::any> progress_duration_values = {0, 1, 2, 3, 4, 5};
static const std::vector<std::string> progress_duration_labels = {"Off", "1s", "2s", "3s", "4s", "5s"};

// Menu transition mode options
static const std::vector<std::any>    transition_mode_values = {(int)TRANSITION_OFF, (int)TRANSITION_SNAPPY, (int)TRANSITION_COMFY};
static const std::vector<std::string> transition_mode_labels = {"Off", "Snappy", "Comfy"};

// Game switcher curtain opacity options (0-100)
static const std::vector<std::any>    curtain_opacity_values = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const std::vector<std::string> curtain_opacity_labels = {"Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"};

// Input prompt style options
static const std::vector<std::any>    input_prompt_style_values = {(int)INPUT_STYLE_TEXT, (int)INPUT_STYLE_ABXY, (int)INPUT_STYLE_CARDINALS, (int)INPUT_STYLE_SHAPES};
static const std::vector<std::string> input_prompt_style_labels = {"Text", "ABXY", "Cardinals", "Shapes"};

// RetroAchievements sort order options
static const std::vector<std::any> ra_sort_values = {
    (int)RA_SORT_UNLOCKED_FIRST,
    (int)RA_SORT_DISPLAY_ORDER_FIRST,
    (int)RA_SORT_DISPLAY_ORDER_LAST,
    (int)RA_SORT_WON_BY_MOST,
    (int)RA_SORT_WON_BY_LEAST,
    (int)RA_SORT_POINTS_MOST,
    (int)RA_SORT_POINTS_LEAST,
    (int)RA_SORT_TITLE_AZ,
    (int)RA_SORT_TITLE_ZA,
    (int)RA_SORT_TYPE_ASC,
    (int)RA_SORT_TYPE_DESC
};
static const std::vector<std::string> ra_sort_labels = {
    "Unlocked First",
    "Display Order (First)",
    "Display Order (Last)",
    "Won By (Most)",
    "Won By (Least)",
    "Points (Most)",
    "Points (Least)",
    "Title (A-Z)",
    "Title (Z-A)",
    "Type (Asc)",
    "Type (Desc)"
};

namespace {
    struct ColorDef { int id; const char *name; const char *desc; uint32_t defaultColor; };
    static const ColorDef g_colorDefs[] = {
        {1, "Selection accent", "Fill for the current selection. Text contrast is automatic.", CFG_DEFAULT_COLOR1},
        {2, "Header surface",   "Dark surface for titles, hints and notifications.",           CFG_DEFAULT_COLOR2},
        {3, "Raised surface",   "Dark surface for cards and unselected controls.",             CFG_DEFAULT_COLOR3},
        {4, "Primary text",     "Main list and settings text.",                                CFG_DEFAULT_COLOR4},
        {5, "Selection text",   "Derived automatically from the selection accent.",            CFG_DEFAULT_COLOR5},
        {6, "Secondary text",   "Hints and supporting information.",                           CFG_DEFAULT_COLOR6},
        {7, "Backdrop",         "The dark background behind the interface.",                   CFG_DEFAULT_COLOR7},
    };

    static std::vector<ColorPreset> buildColorPresets(int excludeId)
    {
        std::vector<ColorPreset> result;
        for (const auto &def : g_colorDefs)
        {
            if (def.id != excludeId && def.id != 5)
                result.push_back({CFG_getColor(def.id), def.name});
        }
        return result;
    }

    std::string execCommand(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;

        // Redirect stderr to stdout using 2>&1
        std::string fullCmd = std::string(cmd) + " 2>&1";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(fullCmd.c_str(), "r"), pclose);
        if (!pipe) throw std::runtime_error("popen() failed!");

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    }

    std::string extractBusyBoxVersion(const std::string& output) {
        std::regex versionRegex(R"(BusyBox\s+(v[\d.]+))");
        std::smatch match;
        if (std::regex_search(output, match, versionRegex)) {
            return match.str(1);
        }
        return "";
    }

    class DeviceInfo {
    public:
        enum Vendor {
            Unknown,
            Trimui,
            Miyoo,
            Anbernic
        };

        enum Model {
            UnknownModel,
            Brick,
            BrickPro,
            SmartPro,
            SmartProS,
            Flip,
            RG40XXV,
            RG40XXH,
            RG34XX,
            RG34XXSP,
            RGSP, // RG34XXSP panel, no sticks
            RG35XXPlus, // Plus and 2024 both report RG35xx
            RG35XXH,
            RG35XXSP,
            RG35XXPRO,
            RG28XX,
            RGCubeXX
        };

        enum Platform {
            UnknownPlatform,
            tg5040,
            tg5050,
            my355,
            h700
        };

        DeviceInfo() {
            char* device = getenv("DEVICE");
            if (device) {
                if(exactMatch("brick", device)) {
                    m_vendor = Trimui;
                    m_model = Brick;
                    m_platform = tg5040;
                } else if(exactMatch("brickpro", device)) {
                    m_vendor = Trimui;
                    m_model = BrickPro;
                    m_platform = tg5040;
                } else if(exactMatch("smartpro", device)) {
                    m_vendor = Trimui;
                    m_model = SmartPro;
                    m_platform = tg5040;
                } else if(exactMatch("smartpros", device)) {
                    m_vendor = Trimui;
                    m_model = SmartProS;
                    m_platform = tg5050;
                } else if(exactMatch("my355", device)) {
                    m_vendor = Trimui;
                    m_model = Flip;
                    m_platform = my355;
                } else if(exactMatch("rg40xxv", device)) {
                    m_vendor = Anbernic;
                    m_model = RG40XXV;
                    m_platform = h700;
                } else if(exactMatch("rg40xxh", device)) {
                    m_vendor = Anbernic;
                    m_model = RG40XXH;
                    m_platform = h700;
                } else if(exactMatch("rg35xxplus", device)) {
                    m_vendor = Anbernic;
                    m_model = RG35XXPlus;
                    m_platform = h700;
                } else if(exactMatch("rg35xxh", device)) {
                    m_vendor = Anbernic;
                    m_model = RG35XXH;
                    m_platform = h700;
                } else if(exactMatch("rg35xxsp", device)) {
                    m_vendor = Anbernic;
                    m_model = RG35XXSP;
                    m_platform = h700;
                } else if(exactMatch("rg35xxpro", device)) {
                    m_vendor = Anbernic;
                    m_model = RG35XXPRO;
                    m_platform = h700;
                } else if(exactMatch("rg34xx", device)) {
                    m_vendor = Anbernic;
                    m_model = RG34XX;
                    m_platform = h700;
                } else if(exactMatch("rg34xxsp", device)) {
                    m_vendor = Anbernic;
                    m_model = RG34XXSP;
                    m_platform = h700;
                } else if(exactMatch("rgsp", device)) {
                    m_vendor = Anbernic;
                    m_model = RGSP;
                    m_platform = h700;
                } else if(exactMatch("rg28xx", device)) {
                    m_vendor = Anbernic;
                    m_model = RG28XX;
                    m_platform = h700;
                } else if(exactMatch("rgcubexx", device)) {
                    m_vendor = Anbernic;
                    m_model = RGCubeXX;
                    m_platform = h700;
                }
            }
        }

        Vendor getVendor() const { return m_vendor; }
        Model getModel() const { return m_model; }
        Platform getPlatform() const { return m_platform; }

        bool hasColorTemperature() const {
            return m_platform == tg5040 || m_platform == h700;
        }

        // the H700 disp driver doesn't support the enhance_* sysfs nodes,
        // so contrast/saturation/exposure are hidden there
        bool hasContrastSaturation() const {
            return m_platform == my355 || m_platform == tg5040;
        }

        bool hasExposure() const {
            return m_platform == tg5040;
        }

        bool hasDisplayCal() const {
            return m_platform == tg5040 || m_platform == h700;
        }

        bool hasActiveCooling() const {
            return m_platform == tg5050;
        }

        bool hasMuteToggle() const {
            return m_platform == tg5050 || m_platform == tg5040;
        }

        bool hasAnalogSticks() const {
            return m_model == SmartPro || m_model == SmartProS || m_model == BrickPro
                || m_model == RG40XXV || m_model == RG40XXH || m_model == RGCubeXX
                || m_model == RG34XXSP || m_model == RG35XXH || m_model == RG35XXPRO;
        }

        bool hasWifi() const {
            return m_platform == tg5050 || m_platform == tg5040 || m_platform == my355 || m_platform == h700;
        }

        bool hasBluetooth() const {
            return m_platform == tg5050 || m_platform == tg5040 || m_platform == my355 || m_platform == h700;
        }

    private:
        Vendor m_vendor = Unknown;
        Model m_model = UnknownModel;
        Platform m_platform = UnknownPlatform;
    };
}
int main(int argc, char *argv[])
{
    try
    {
        DeviceInfo deviceInfo;

        char version[128];
        PLAT_getOsVersionInfo(version, 128);
        LOG_info("This is stock OS version %s\n", version);
        InitSettings();

        PWR_setCPUSpeed(CPU_SPEED_AUTO);

        Context ctx = {0};
        ctx.dirty = 1;
        ctx.show_setting = 0;
        ctx.screen = GFX_init(MODE_MAIN);
        PAD_init();
        PWR_init();
        TIME_init();

        signal(SIGINT, sigHandler);
        signal(SIGTERM, sigHandler);

        char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
        int tz_count = 0;
        TIME_getTimezones(timezones, &tz_count);

        int was_online = PWR_isOnline();
        int had_bt = PLAT_btIsConnected();

        std::vector<std::any> tz_values;
        std::vector<std::string> tz_labels;
        for (int i = 0; i < tz_count; ++i) {
            //LOG_info("Timezone: %s\n", timezones[i]);
            tz_values.push_back(std::string(timezones[i]));
            // Todo: beautify, remove underscores and so on
            tz_labels.push_back(std::string(timezones[i]));
        }

        // Factory helpers to avoid repeating identical lambda boilerplate for each picker.
        // Editing an individual color detaches from any predefined palette ("Custom").
        auto makeColorSetter = [](int id) -> ValueSetCallback {
            return [id](const std::any &v){ CFG_setColor(id, std::any_cast<uint32_t>(v)); CFG_clearPalette(); };
        };
        auto makeColorOpener = [](ColorPickerMenu *picker, int id, std::string name) -> MenuListCallback {
            return [picker, id, name](AbstractMenuItem &item) -> InputReactionHint {
                picker->reset(CFG_getColor(id), buildColorPresets(id), name);
                return DeferToSubmenu(item);
            };
        };
        auto makeColorGetter = [](int id) -> ValueGetCallback {
            return [id]() -> std::any { return CFG_getColor(id); };
        };
        auto makeColorResetter = [](int id, uint32_t defaultColor) -> ValueResetCallback {
            return [id, defaultColor]() { CFG_setColor(id, defaultColor); CFG_clearPalette(); };
        };

        // Pre-create one RGBA picker per color setting (reused across opens)
        std::vector<std::unique_ptr<ColorPickerMenu>> pickers;
        pickers.reserve(std::size(g_colorDefs));
        for (const auto &def : g_colorDefs) {
            if (def.id == 5) continue;
            pickers.push_back(std::make_unique<ColorPickerMenu>(
                CFG_getColor(def.id), makeColorSetter(def.id), buildColorPresets(def.id), def.name));
        }

        // Build color MenuItems (loop order = g_colorDefs order = display order)
        std::vector<AbstractMenuItem *> colorMenuItems;
        colorMenuItems.reserve(std::size(g_colorDefs));
        for (int i = 0; i < (int)std::size(g_colorDefs); i++)
        {
            const auto &def = g_colorDefs[i];
            if (def.id == 5) continue;
            ColorPickerMenu *picker = pickers.back().get();
            colorMenuItems.push_back(new MenuItem{
                ListItemType::Color, def.name, def.desc, colors, color_strings,
                makeColorGetter(def.id),
                makeColorSetter(def.id),
                makeColorResetter(def.id, def.defaultColor),
                makeColorOpener(picker, def.id, def.name), picker});
        }

        std::vector<AbstractMenuItem *> appearanceItems;
        auto fonts = enumerateFonts();
        std::vector<std::any> font_values;
        std::vector<std::string> font_labels;
        for (const auto &f : fonts) {
            font_values.push_back(f.filename);
            font_labels.push_back(f.label);
        }
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Font", "The font to render all UI text.", font_values, font_labels,
            []() -> std::any { return std::string(CFG_getFontFile()); },
            [](const std::any &value) { CFG_setFontFile(std::any_cast<std::string>(value).c_str()); },
            []() { CFG_setFontFile(CFG_DEFAULT_FONT_FILE); }});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Font style", "The style to render the UI font (e.g. bold)", std::vector<std::any>{0, 1}, std::vector<std::string>{"Normal", "Bold"},
            []() -> std::any { return CFG_getFontStyle(); },
            [](const std::any &value) { CFG_setFontStyle(std::any_cast<int>(value)); },
            []() { CFG_setFontStyle(CFG_DEFAULT_FONT_STYLE); }});
        static const std::vector<std::any> font_size_values = {0, 1, 2, 3};
        static const std::vector<std::string> font_size_labels = {"Compact", "Comfortable", "Large", "Extra Large"};
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Font size", "The size to render the UI and gamelist font", font_size_values, font_size_labels,
            []() -> std::any { return CFG_getFontSize(); },
            [](const std::any &value) { CFG_setFontSize(std::any_cast<int>(value)); },
            []() { CFG_setFontSize(CFG_DEFAULT_FONT_SIZE); }});
        appearanceItems.push_back(buildPaletteMenuItem());
        for (auto *item : colorMenuItems)
            appearanceItems.push_back(item);
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show battery percentage", "Show battery level as percent in the status pill", {false, true}, on_off,
            []() -> std::any { return CFG_getShowBatteryPercent(); },
            [](const std::any &value) { CFG_setShowBatteryPercent(std::any_cast<bool>(value)); },
            []() { CFG_setShowBatteryPercent(CFG_DEFAULT_SHOWBATTERYPERCENT);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show menu animations", "Enable or disable menu animations", {false, true}, on_off,
            []() -> std::any{ return CFG_getMenuAnimations(); },
            [](const std::any &value) { CFG_setMenuAnimations(std::any_cast<bool>(value)); },
            []() { CFG_setMenuAnimations(CFG_DEFAULT_SHOWMENUANIMATIONS);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Menu transitions", "Style of animated transition when navigating menus", transition_mode_values, transition_mode_labels,
            []() -> std::any { return CFG_getMenuTransitions(); },
            [](const std::any &value) { CFG_setMenuTransitions(std::any_cast<int>(value)); },
            []() { CFG_setMenuTransitions(CFG_DEFAULT_SHOWMENUTRANSITIONS); }});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Game art corner radius", "Set the radius for the rounded corners of game art", 0, 24, "px",
            []() -> std::any{ return CFG_getThumbnailRadius(); },
            [](const std::any &value) { CFG_setThumbnailRadius(std::any_cast<int>(value)); },
            []() { CFG_setThumbnailRadius(CFG_DEFAULT_THUMBRADIUS);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Game art width", "Set the percentage of screen width used for game art.\nUI elements might overrule this to avoid clipping.",
            5, 100, "%",
            []() -> std::any{ return (int)(CFG_getGameArtWidth() * 100); },
            [](const std::any &value) { CFG_setGameArtWidth((double)std::any_cast<int>(value) / 100.0); },
            []() { CFG_setGameArtWidth(CFG_DEFAULT_GAMEARTWIDTH);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show folder names at root", "Show folder names at root directory", {false, true}, on_off,
            []() -> std::any { return CFG_getShowFolderNamesAtRoot(); },
            [](const std::any &value) { CFG_setShowFolderNamesAtRoot(std::any_cast<bool>(value)); },
            []() { CFG_setShowFolderNamesAtRoot(CFG_DEFAULT_SHOWFOLDERNAMESATROOT);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show Recents", "Show \"Recently Played\" menu entry in game list.", {false, true}, on_off,
            []() -> std::any { return CFG_getShowRecents(); },
            [](const std::any &value) { CFG_setShowRecents(std::any_cast<bool>(value)); },
            []() { CFG_setShowRecents(CFG_DEFAULT_SHOWRECENTS);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show Tools", "Show \"Tools\" menu entry in game list.\nMENU opens Settings; START opens the Quick Menu.", {false, true}, on_off,
            []() -> std::any { return CFG_getShowTools(); },
            [](const std::any &value) { CFG_setShowTools(std::any_cast<bool>(value)); },
            []() { CFG_setShowTools(CFG_DEFAULT_SHOWTOOLS);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show game art", "Show game artwork in the main menu", {false, true}, on_off,
            []() -> std::any { return CFG_getShowGameArt(); },
            [](const std::any &value) { CFG_setShowGameArt(std::any_cast<bool>(value)); },
            []() { CFG_setShowGameArt(CFG_DEFAULT_SHOWGAMEART);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Use folder background for ROMs", "If enabled, used the emulator background image. Otherwise uses the default.", {false, true}, on_off,
            []() -> std::any { return CFG_getRomsUseFolderBackground(); },
            [](const std::any &value) { CFG_setRomsUseFolderBackground(std::any_cast<bool>(value)); },
            []() { CFG_setRomsUseFolderBackground(CFG_DEFAULT_ROMSUSEFOLDERBACKGROUND);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Show Quickswitcher UI", "Show/hide Quickswitcher UI elements.\nWhen hidden, will only draw background images.", {false, true}, on_off,
            []() -> std::any{ return CFG_getShowQuickswitcherUI(); },
            [](const std::any &value){ CFG_setShowQuickswitcherUI(std::any_cast<bool>(value)); },
            []() { CFG_setShowQuickswitcherUI(CFG_DEFAULT_SHOWQUICKWITCHERUI);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Game switcher curtain opacity", "Show/hide curtain overlay. Helps UI elements to \nstand out when using transparent backgrounds.", curtain_opacity_values, curtain_opacity_labels, 
            []() -> std::any{ return CFG_getGameSwitcherCurtain(); },
            [](const std::any &value){ CFG_setGameSwitcherCurtain(std::any_cast<int>(value)); },
            []() { CFG_setGameSwitcherCurtain(CFG_DEFAULT_GAMESWITCHER_CURTAIN);}});
        appearanceItems.push_back(new MenuItem{ListItemType::Generic, "Input prompt style", "Select the style of input prompts.", input_prompt_style_values, input_prompt_style_labels,
            []() -> std::any{ return CFG_getInputPromptStyle(); },
            [](const std::any &value){ CFG_setInputPromptStyle(std::any_cast<int>(value)); },
            []() { CFG_setInputPromptStyle(CFG_DEFAULT_INPUT_PROMPT_STYLE);}});
        // not needed anymore
        // new MenuItem{ListItemType::Generic, "Game switcher scaling", "The scaling algorithm used to display the savegame image.", scaling, scaling_strings, []() -> std::any
        // { return CFG_getGameSwitcherScaling(); },
        // [](const std::any &value)
        // { CFG_setGameSwitcherScaling(std::any_cast<int>(value)); },
        // []() { CFG_setGameSwitcherScaling(CFG_DEFAULT_GAMESWITCHERSCALING);}},
        appearanceItems.push_back(new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu});
        auto *appearanceMenu = new MenuList(MenuItemType::Fixed, "Appearance", std::move(appearanceItems));

        std::vector<AbstractMenuItem*> displayItems = {
            new MenuItem{ListItemType::Generic, "Brightness", "Display brightness (0 to 10)", 0, 10, "",[]() -> std::any
            { return GetBrightness(); }, [](const std::any &value)
            { SetBrightness(std::any_cast<int>(value)); },
            []() { SetBrightness(SETTINGS_DEFAULT_BRIGHTNESS);}},

        };

        if(deviceInfo.hasColorTemperature())
        {
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Color temperature", "Color temperature (0 to 40)", 0, 40, "",[]() -> std::any
                { return GetColortemp(); }, [](const std::any &value)
                { SetColortemp(std::any_cast<int>(value)); },
                []() { SetColortemp(SETTINGS_DEFAULT_COLORTEMP);}});
        }

        if(deviceInfo.hasContrastSaturation())
        {
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Contrast", "Contrast enhancement (-4 to 5)", -4, 5, "",[]() -> std::any
                { return GetContrast(); }, [](const std::any &value)
                { SetContrast(std::any_cast<int>(value)); },
                []() { SetContrast(SETTINGS_DEFAULT_CONTRAST);}});
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Saturation", "Saturation enhancement (-5 to 5)", -5, 5, "",[]() -> std::any
                { return GetSaturation(); }, [](const std::any &value)
                { SetSaturation(std::any_cast<int>(value)); },
                []() { SetSaturation(SETTINGS_DEFAULT_SATURATION);}});
        }

        if(deviceInfo.hasExposure())
        {
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Exposure", "Exposure enhancement (-4 to 5)", -4, 5, "",[]() -> std::any
                { return GetExposure(); }, [](const std::any &value)
                { SetExposure(std::any_cast<int>(value)); },
                []() { SetExposure(SETTINGS_DEFAULT_EXPOSURE);}});
        }
        if(deviceInfo.hasDisplayCal())
        {
            const DisplayCalDefaults defaultDisplayCal = DisplayCal_getDefaultSettings(
                deviceInfo.getModel() == DeviceInfo::Brick ? DISPLAYCAL_PRESET_BRICK :
                deviceInfo.getModel() == DeviceInfo::BrickPro ? DISPLAYCAL_PRESET_BRICKPRO :
                deviceInfo.getModel() == DeviceInfo::SmartPro ? DISPLAYCAL_PRESET_SMARTPRO :
                deviceInfo.getModel() == DeviceInfo::RG28XX ? DISPLAYCAL_PRESET_RG28XX :
                deviceInfo.getModel() == DeviceInfo::RG34XX ? DISPLAYCAL_PRESET_RG34XX :
                deviceInfo.getModel() == DeviceInfo::RG34XXSP ? DISPLAYCAL_PRESET_RG34XXSP :
                deviceInfo.getModel() == DeviceInfo::RGSP ? DISPLAYCAL_PRESET_RGSP :
                deviceInfo.getModel() == DeviceInfo::RG35XXPlus ? DISPLAYCAL_PRESET_RG35XXPLUS :
                deviceInfo.getModel() == DeviceInfo::RG35XXH ? DISPLAYCAL_PRESET_RG35XXH :
                deviceInfo.getModel() == DeviceInfo::RG35XXSP ? DISPLAYCAL_PRESET_RG35XXSP :
                deviceInfo.getModel() == DeviceInfo::RG35XXPRO ? DISPLAYCAL_PRESET_RG35XXPRO :
                deviceInfo.getModel() == DeviceInfo::RG40XXH ? DISPLAYCAL_PRESET_RG40XXH :
                deviceInfo.getModel() == DeviceInfo::RG40XXV ? DISPLAYCAL_PRESET_RG40XXV :
                deviceInfo.getModel() == DeviceInfo::RGCubeXX ? DISPLAYCAL_PRESET_RGCUBEXX :
                DISPLAYCAL_PRESET_DEFAULT);
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "White point correction", "Corrects the display white point to better match the \nsRGB standard, at the expense of some peak brightness.", {false, true}, on_off, []() -> std::any
                { return GetDisplayCalEnabled() != 0; }, [](const std::any &value)
                { SetDisplayCalEnabled(std::any_cast<bool>(value)); },
                [defaultDisplayCal]() { SetDisplayCalEnabled(defaultDisplayCal.enabled); }});
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Red gain", "White point correction red channel gain (0 to 200)", DISPLAYCAL_GAIN_MIN, DISPLAYCAL_GAIN_MAX, "", []() -> std::any
                { return GetDisplayCalRedGain(); }, [](const std::any &value)
                { SetDisplayCalRedGain(std::any_cast<int>(value)); },
                [defaultDisplayCal]() { SetDisplayCalRedGain(defaultDisplayCal.red_gain); }});
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Green gain", "White point correction green channel gain (0 to 200)", DISPLAYCAL_GAIN_MIN, DISPLAYCAL_GAIN_MAX, "", []() -> std::any
                { return GetDisplayCalGreenGain(); }, [](const std::any &value)
                { SetDisplayCalGreenGain(std::any_cast<int>(value)); },
                [defaultDisplayCal]() { SetDisplayCalGreenGain(defaultDisplayCal.green_gain); }});
            displayItems.push_back(
                new MenuItem{ListItemType::Generic, "Blue gain", "White point correction blue channel gain (0 to 200)", DISPLAYCAL_GAIN_MIN, DISPLAYCAL_GAIN_MAX, "", []() -> std::any
                { return GetDisplayCalBlueGain(); }, [](const std::any &value)
                { SetDisplayCalBlueGain(std::any_cast<int>(value)); },
                [defaultDisplayCal]() { SetDisplayCalBlueGain(defaultDisplayCal.blue_gain); }});
        }
        displayItems.push_back(
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu});

        auto displayMenu = new MenuList(MenuItemType::Fixed, "Display", displayItems);

        std::vector<AbstractMenuItem*> systemItems = {
            new MenuItem{ListItemType::Generic, "Volume", "Speaker volume",
            {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
            {"Muted", "5%","10%","15%","20%","25%","30%","35%","40%","45%","50%","55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"},
            []() -> std::any{ return GetVolume(); }, [](const std::any &value)
            { SetVolume(std::any_cast<int>(value)); },
            []() { SetVolume(SETTINGS_DEFAULT_VOLUME);}},
            new MenuItem{ListItemType::Generic, "Screen timeout", "Period of inactivity before screen turns off (0-600s)", screen_timeout_secs, screen_timeout_labels, []() -> std::any
            { return CFG_getScreenTimeoutSecs(); }, [](const std::any &value)
            { CFG_setScreenTimeoutSecs(std::any_cast<uint32_t>(value)); },
            []() { CFG_setScreenTimeoutSecs(CFG_DEFAULT_SCREENTIMEOUTSECS);}},
            new MenuItem{ListItemType::Generic, "Suspend timeout", "Time before device goes to sleep after screen is off (5-600s)", sleep_timeout_secs, sleep_timeout_labels, []() -> std::any
            { return CFG_getSuspendTimeoutSecs(); }, [](const std::any &value)
            { CFG_setSuspendTimeoutSecs(std::any_cast<uint32_t>(value)); },
            []() { CFG_setSuspendTimeoutSecs(CFG_DEFAULT_SUSPENDTIMEOUTSECS);}},
            new MenuItem{ListItemType::Generic, "Haptic feedback", "Enable or disable haptic feedback on certain actions in the OS", {false, true}, on_off, []() -> std::any
            { return CFG_getHaptics(); }, [](const std::any &value)
            { CFG_setHaptics(std::any_cast<bool>(value)); },
            []() { CFG_setHaptics(CFG_DEFAULT_HAPTICS);}},
            new MenuItem{ListItemType::Generic, "Default view", "The initial view to show on boot",
            {(int)SCREEN_GAMELIST, (int)SCREEN_GAMESWITCHER, (int)SCREEN_QUICKMENU},
            {"Content List","Game Switcher","Quick Menu"},
            []() -> std::any { return CFG_getDefaultView(); },
            [](const std::any &value){ CFG_setDefaultView(std::any_cast<int>(value)); },
            []() { CFG_setDefaultView(CFG_DEFAULT_VIEW);}},
            new MenuItem{ListItemType::Generic, "Show 24h time format", "Show clock in the 24hrs time format", {false, true}, on_off, []() -> std::any
            { return CFG_getClock24H(); },
            [](const std::any &value)
            { CFG_setClock24H(std::any_cast<bool>(value)); },
            []() { CFG_setClock24H(CFG_DEFAULT_CLOCK24H);}},
            new MenuItem{ListItemType::Generic, "Show clock", "Show clock in the status pill", {false, true}, on_off, []() -> std::any
            { return CFG_getShowClock(); },
            [](const std::any &value)
            { CFG_setShowClock(std::any_cast<bool>(value)); },
            []() { CFG_setShowClock(CFG_DEFAULT_SHOWCLOCK);}},
            new MenuItem{ListItemType::Generic, "Set time and date automatically", "Automatically adjust system time\nwith NTP (requires internet access)", {false, true}, on_off, []() -> std::any
            { return TIME_getNetworkTimeSync(); }, [](const std::any &value)
            { TIME_setNetworkTimeSync(std::any_cast<bool>(value)); },
            []() { TIME_setNetworkTimeSync(false);}}, // default from stock
            new MenuItem{ListItemType::Generic, "Time zone", "Your time zone", tz_values, tz_labels, []() -> std::any
            { return std::string(TIME_getCurrentTimezone()); }, [](const std::any &value)
            { TIME_setCurrentTimezone(std::any_cast<std::string>(value).c_str()); },
            []() { TIME_setCurrentTimezone("Asia/Shanghai");}}, // default from Stock
            new MenuItem{ListItemType::Generic, "Save format", "The save format to use.\nMinUI: Game.gba.sav, Retroarch: Game.srm, Generic: Game.sav",
            {(int)SAVE_FORMAT_SAV, (int)SAVE_FORMAT_SRM, (int)SAVE_FORMAT_SRM_UNCOMPRESSED, (int)SAVE_FORMAT_GEN},
            {"MinUI (default)", "Retroarch (compressed)", "Retroarch (uncompressed)", "Generic"}, []() -> std::any
            { return CFG_getSaveFormat(); }, [](const std::any &value)
            { CFG_setSaveFormat(std::any_cast<int>(value)); },
            []() { CFG_setSaveFormat(CFG_DEFAULT_SAVEFORMAT);}},
            new MenuItem{ListItemType::Generic, "Save state format", "The save state format to use. MinUI: Game.st0, \nRetroarch-ish: Game.state.0, Retroarch: Game.state0",
            {(int)STATE_FORMAT_SAV, (int)STATE_FORMAT_SRM_EXTRADOT, (int)STATE_FORMAT_SRM_UNCOMRESSED_EXTRADOT, (int)STATE_FORMAT_SRM, (int)STATE_FORMAT_SRM_UNCOMRESSED},
            {"MinUI (default)", "Retroarch-ish (compressed)", "Retroarch-ish (uncompressed)", "Retroarch (compressed)", "Retroarch (uncompressed)"}, []() -> std::any
            { return CFG_getStateFormat(); }, [](const std::any &value)
            { CFG_setStateFormat(std::any_cast<int>(value)); },
            []() { CFG_setStateFormat(CFG_DEFAULT_STATEFORMAT);}},
            new MenuItem{ListItemType::Generic, "Use extracted file name", "Use the extracted file name instead of the archive name.\nOnly applies to cores that do not handle archives natively", {false, true}, on_off,
            []() -> std::any{ return CFG_getUseExtractedFileName(); },
            [](const std::any &value){ CFG_setUseExtractedFileName(std::any_cast<bool>(value)); },
            []() { CFG_setUseExtractedFileName(CFG_DEFAULT_EXTRACTEDFILENAME);}}
        };

        if(deviceInfo.getPlatform() == DeviceInfo::tg5040)
        {
            systemItems.push_back(
                new MenuItem{ListItemType::Generic, "Safe poweroff", "Bypasses the stock shutdown procedure to avoid the \"limbo bug\".\nInstructs the PMIC directly to soft disconnect the battery.", {false, true}, on_off,
                []() -> std::any { return CFG_getPowerOffProtection(); },
                [](const std::any &value) { CFG_setPowerOffProtection(std::any_cast<bool>(value)); },
                []() { CFG_setPowerOffProtection(CFG_DEFAULT_POWEROFFPROTECTION); }}
            );
        }

        if(deviceInfo.getPlatform() == DeviceInfo::tg5040 || deviceInfo.getPlatform() == DeviceInfo::h700)
        {
            systemItems.push_back(
                new MenuItem{ListItemType::Generic, "Keep awake over USB", "Prevent screen-off and sleep while connected to a\ncomputer as a USB device (not just charging).", {false, true}, on_off,
                []() -> std::any { return CFG_getKeepAwakeWhenUSB(); },
                [](const std::any &value) { CFG_setKeepAwakeWhenUSB(std::any_cast<bool>(value)); },
                []() { CFG_setKeepAwakeWhenUSB(CFG_DEFAULT_KEEPAWAKEWHENUSB); }}
            );
        }

        if(deviceInfo.hasActiveCooling())
        {
            systemItems.push_back(
                new MenuItem{ListItemType::Generic, "Fan Speed", "Select the fan speed percentage (Quiet/Normal/Performance or 0-100%)",
                {-3,-2,-1,0,10,20,30,40,50,60,70,80,90,100}, {"Performance","Normal","Quiet","0%","10%","20%","30%","40%","50%","60%","70%","80%","90%","100%"},
                []() -> std::any { return GetFanSpeed(); },
                [](const std::any &value){ SetFanSpeed(std::any_cast<int>(value)); },
                []() { SetFanSpeed(SETTINGS_DEFAULT_FAN_SPEED); }}
            );
        }

        systemItems.push_back(
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu});

        auto systemMenu = new MenuList(MenuItemType::Fixed, "System", systemItems);

        std::vector<AbstractMenuItem*> muteItems =
        {
            new MenuItem{ListItemType::Generic, "Volume when toggled", "Speaker volume (0-20)",
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
            {"Unchanged", "Muted", "5%","10%","15%","20%","25%","30%","35%","40%","45%","50%","55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"},
            []() -> std::any { return GetMutedVolume(); },
            [](const std::any &value) { SetMutedVolume(std::any_cast<int>(value)); },
            []() { SetMutedVolume(0); }},
            new MenuItem{ListItemType::Generic, "FN switch disables LED", "Switch will also disable LEDs", {false, true}, on_off,
            []() -> std::any { return CFG_getMuteLEDs(); },
            [](const std::any &value) { CFG_setMuteLEDs(std::any_cast<bool>(value)); },
            []() { CFG_setMuteLEDs(CFG_DEFAULT_MUTELEDS); }},
            new MenuItem{ListItemType::Generic, "Brightness when toggled", "Display brightness (0 to 10)",
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10},
            {"Unchanged","0","1","2","3","4","5","6","7","8","9","10"},
            []() -> std::any { return GetMutedBrightness(); }, [](const std::any &value)
            { SetMutedBrightness(std::any_cast<int>(value)); },
            []() { SetMutedBrightness(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
        };

        if(deviceInfo.hasMuteToggle())
        {
            if(deviceInfo.hasColorTemperature()) {
                muteItems.push_back(
                    new MenuItem{ListItemType::Generic, "Color temperature when toggled", "Color temperature (0 to 40)",
                    {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40},
                    {"Unchanged","0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40"},
                    []() -> std::any{ return GetMutedColortemp(); }, [](const std::any &value)
                    { SetMutedColortemp(std::any_cast<int>(value)); },
                    []() { SetMutedColortemp(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}}
                );
            }
            if(deviceInfo.hasContrastSaturation()) {
                muteItems.insert(muteItems.end(), {
                    new MenuItem{ListItemType::Generic, "Contrast when toggled", "Contrast enhancement (-4 to 5)",
                    {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -4,-3,-2,-1,0,1,2,3,4,5},
                    {"Unchanged","-4","-3","-2","-1","0","1","2","3","4","5"},
                    []() -> std::any  { return GetMutedContrast(); }, [](const std::any &value)
                    { SetMutedContrast(std::any_cast<int>(value)); },
                    []() { SetMutedContrast(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
                    new MenuItem{ListItemType::Generic, "Saturation when toggled", "Saturation enhancement (-5 to 5)",
                    {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -5,-4,-3,-2,-1,0,1,2,3,4,5},
                    {"Unchanged","-5","-4","-3","-2","-1","0","1","2","3","4","5"},
                    []() -> std::any{ return GetMutedSaturation(); }, [](const std::any &value)
                    { SetMutedSaturation(std::any_cast<int>(value)); },
                    []() { SetMutedSaturation(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}}}
                );
            }
            if(deviceInfo.hasExposure()) {
                muteItems.push_back(
                    new MenuItem{ListItemType::Generic, "Exposure when toggled", "Exposure enhancement (-4 to 5)",
                    {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -4,-3,-2,-1,0,1,2,3,4,5},
                    {"Unchanged","-4","-3","-2","-1","0","1","2","3","4","5"},
                    []() -> std::any  { return GetMutedExposure(); }, [](const std::any &value)
                    { SetMutedExposure(std::any_cast<int>(value)); },
                    []() { SetMutedExposure(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}}
                );
            }

            muteItems.insert(muteItems.end(), {
                new MenuItem{ListItemType::Generic, "Turbo fire A", "Enable turbo fire A", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboA(); },
                [](const std::any &value) { SetMuteTurboA(std::any_cast<int>(value));},
                []() { SetMuteTurboA(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire B", "Enable turbo fire B", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboB(); },
                [](const std::any &value) { SetMuteTurboB(std::any_cast<int>(value));},
                []() { SetMuteTurboB(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire X", "Enable turbo fire X", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboX(); },
                [](const std::any &value) { SetMuteTurboX(std::any_cast<int>(value));},
                []() { SetMuteTurboX(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire Y", "Enable turbo fire Y", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboY(); },
                [](const std::any &value) { SetMuteTurboY(std::any_cast<int>(value));},
                []() { SetMuteTurboY(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire L1", "Enable turbo fire L1", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboL1(); },
                [](const std::any &value) { SetMuteTurboL1(std::any_cast<int>(value));},
                []() { SetMuteTurboL1(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire L2", "Enable turbo fire L2", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboL2(); },
                [](const std::any &value) { SetMuteTurboL2(std::any_cast<int>(value));},
                []() { SetMuteTurboL2(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire R1", "Enable turbo fire R1", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboR1(); },
                [](const std::any &value) { SetMuteTurboR1(std::any_cast<int>(value));},
                []() { SetMuteTurboR1(0);}},
                new MenuItem{ListItemType::Generic, "Turbo fire R2", "Enable turbo fire R2", {0, 1}, on_off, []() -> std::any
                { return GetMuteTurboR2(); },
                [](const std::any &value) { SetMuteTurboR2(std::any_cast<int>(value));},
                []() { SetMuteTurboR2(0);}}
            });
        }

        if(deviceInfo.hasMuteToggle() && !deviceInfo.hasAnalogSticks()){
            muteItems.push_back(
                new MenuItem{ListItemType::Generic, "Dpad mode when toggled", "Dpad: default. Joystick: Dpad exclusively acts as analog stick.\nBoth: Dpad and Joystick inputs at the same time.", {0, 1, 2}, {"Dpad", "Joystick", "Both"}, []() -> std::any
                {
                    if(!GetMuteDisablesDpad() && !GetMuteEmulatesJoystick()) return 0;
                    if(GetMuteDisablesDpad() && GetMuteEmulatesJoystick()) return 1;
                    return 2;
                },
                [](const std::any &value)
                {
                    int v = std::any_cast<int>(value);
                    SetMuteDisablesDpad((v == 1));
                    SetMuteEmulatesJoystick((v > 0));
                },
                []()
                {
                    SetMuteDisablesDpad(0);
                    SetMuteEmulatesJoystick(0);
                }});
        }
        muteItems.push_back(new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu});

        auto notificationsMenu = new MenuList(MenuItemType::Fixed, "Notifications",
        {
            new MenuItem{ListItemType::Generic, "Save states", "Show notification when saving game state", {false, true}, on_off,
            []() -> std::any { return CFG_getNotifyManualSave(); },
            [](const std::any &value) { CFG_setNotifyManualSave(std::any_cast<bool>(value)); },
            []() { CFG_setNotifyManualSave(CFG_DEFAULT_NOTIFY_MANUAL_SAVE);}},
            new MenuItem{ListItemType::Generic, "Load states", "Show notification when loading game state", {false, true}, on_off,
            []() -> std::any { return CFG_getNotifyLoad(); },
            [](const std::any &value) { CFG_setNotifyLoad(std::any_cast<bool>(value)); },
            []() { CFG_setNotifyLoad(CFG_DEFAULT_NOTIFY_LOAD);}},
            new MenuItem{ListItemType::Generic, "Screenshots", "Show notification when taking a screenshot", {false, true}, on_off,
            []() -> std::any { return CFG_getNotifyScreenshot(); },
            [](const std::any &value) { CFG_setNotifyScreenshot(std::any_cast<bool>(value)); },
            []() { CFG_setNotifyScreenshot(CFG_DEFAULT_NOTIFY_SCREENSHOT);}},
            new MenuItem{ListItemType::Generic, "Vol / Display Adjustments", "Show overlay for volume, brightness,\nand color temp adjustments", {false, true}, on_off,
            []() -> std::any { return CFG_getNotifyAdjustments(); },
            [](const std::any &value) { CFG_setNotifyAdjustments(std::any_cast<bool>(value)); },
            []() { CFG_setNotifyAdjustments(CFG_DEFAULT_NOTIFY_ADJUSTMENTS);}},
            new MenuItem{ListItemType::Generic, "Duration", "How long notifications stay on screen", notify_duration_values, notify_duration_labels,
            []() -> std::any { return CFG_getNotifyDuration(); },
            [](const std::any &value) { CFG_setNotifyDuration(std::any_cast<int>(value)); },
            []() { CFG_setNotifyDuration(CFG_DEFAULT_NOTIFY_DURATION);}},
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu},
        });

        // RetroAchievements keyboard prompts
        auto raUsernamePrompt = new KeyboardPrompt("Enter Username", [](AbstractMenuItem &item) -> InputReactionHint {
            CFG_setRAUsername(item.getName().c_str());
            return Exit;
        });

        auto raPasswordPrompt = new KeyboardPrompt("Enter Password", [](AbstractMenuItem &item) -> InputReactionHint {
            CFG_setRAPassword(item.getName().c_str());
            return Exit;
        });

        auto retroAchievementsMenu = new MenuList(MenuItemType::Fixed, "RetroAchievements",
        {
            new MenuItem{ListItemType::Generic, "Enable Achievements", "Enable RetroAchievements integration", {false, true}, on_off,
            []() -> std::any { return CFG_getRAEnable(); },
            [](const std::any &value) { CFG_setRAEnable(std::any_cast<bool>(value)); },
            []() { CFG_setRAEnable(CFG_DEFAULT_RA_ENABLE);}},
            new TextInputMenuItem{"Username", "RetroAchievements username",
            []() -> std::any {
                std::string username = CFG_getRAUsername();
                return username.empty() ? std::string("(not set)") : username;
            },
            [raUsernamePrompt](AbstractMenuItem &item) -> InputReactionHint {
                raUsernamePrompt->setInitialText(CFG_getRAUsername());
                item.defer(true);
                return NoOp;
            }, raUsernamePrompt},
            new TextInputMenuItem{"Password", "RetroAchievements password",
            []() -> std::any {
                std::string password = CFG_getRAPassword();
                return password.empty() ? std::string("(not set)") : std::string("********");
            },
            [raPasswordPrompt](AbstractMenuItem &item) -> InputReactionHint {
                raPasswordPrompt->setInitialText(CFG_getRAPassword());
                item.defer(true);
                return NoOp;
            }, raPasswordPrompt},
            new MenuItem{ListItemType::Button, "Authenticate", "Test credentials and retrieve API token",
            [](AbstractMenuItem &item) -> InputReactionHint {
                const char* username = CFG_getRAUsername();
                const char* password = CFG_getRAPassword();

                if (!username || strlen(username) == 0 || !password || strlen(password) == 0) {
                    item.setDesc("Error: Username and password required");
                    return NoOp;
                }

                item.setDesc("Authenticating...");

                RA_AuthResponse response;
                RA_AuthResult result = RA_authenticateSync(username, password, &response);

                if (result == RA_AUTH_SUCCESS) {
                    CFG_setRAToken(response.token);
                    CFG_setRAAuthenticated(true);
                    std::string desc = "Authenticated as " + std::string(response.display_name);
                    item.setDesc(desc);
                } else {
                    CFG_setRAToken("");
                    CFG_setRAAuthenticated(false);
                    std::string desc = "Error: " + std::string(response.error_message);
                    item.setDesc(desc);
                }
                return NoOp;
            }},
            new StaticMenuItem{ListItemType::Generic, "Status", "Authentication status",
            []() -> std::any {
                if (CFG_getRAAuthenticated() && strlen(CFG_getRAToken()) > 0) {
                    return std::string("Authenticated");
                }
                return std::string("Not authenticated");
            }},
            // TODO: Hardcore mode hidden until feature is fully implemented and ready for the emulator approval process done by the RetroAchievements team
            // new MenuItem{ListItemType::Generic, "Hardcore Mode", "Disable save states and cheats for achievements", {false, true}, on_off,
            // []() -> std::any { return CFG_getRAHardcoreMode(); },
            // [](const std::any &value) { CFG_setRAHardcoreMode(std::any_cast<bool>(value)); },
            // []() { CFG_setRAHardcoreMode(CFG_DEFAULT_RA_HARDCOREMODE);}},
            new MenuItem{ListItemType::Generic, "Show Notifications", "Show achievement unlock notifications", {false, true}, on_off,
            []() -> std::any { return CFG_getRAShowNotifications(); },
            [](const std::any &value) { CFG_setRAShowNotifications(std::any_cast<bool>(value)); },
            []() { CFG_setRAShowNotifications(CFG_DEFAULT_RA_SHOW_NOTIFICATIONS);}},
            new MenuItem{ListItemType::Generic, "Show Challenge Indicators", "Show a trophy indicator while a challenge achievement is active", {false, true}, on_off,
            []() -> std::any { return CFG_getRAShowChallengeIndicators(); },
            [](const std::any &value) { CFG_setRAShowChallengeIndicators(std::any_cast<bool>(value)); },
            []() { CFG_setRAShowChallengeIndicators(CFG_DEFAULT_RA_SHOW_CHALLENGE_INDICATORS);}},
            new MenuItem{ListItemType::Generic, "Notification Duration", "How long achievement notifications stay on screen", notify_duration_values, notify_duration_labels,
            []() -> std::any { return CFG_getRANotificationDuration(); },
            [](const std::any &value) { CFG_setRANotificationDuration(std::any_cast<int>(value)); },
            []() { CFG_setRANotificationDuration(CFG_DEFAULT_RA_NOTIFICATION_DURATION);}},
            new MenuItem{ListItemType::Generic, "Progress Duration", "Duration for progress updates (top-left). Off to disable.", progress_duration_values, progress_duration_labels,
            []() -> std::any { return CFG_getRAProgressNotificationDuration(); },
            [](const std::any &value) { CFG_setRAProgressNotificationDuration(std::any_cast<int>(value)); },
            []() { CFG_setRAProgressNotificationDuration(CFG_DEFAULT_RA_PROGRESS_NOTIFICATION_DURATION);}},
            new MenuItem{ListItemType::Generic, "Achievement Sort Order", "How achievements are sorted in the in-game menu", ra_sort_values, ra_sort_labels,
            []() -> std::any { return CFG_getRAAchievementSortOrder(); },
            [](const std::any &value) { CFG_setRAAchievementSortOrder(std::any_cast<int>(value)); },
            []() { CFG_setRAAchievementSortOrder(CFG_DEFAULT_RA_ACHIEVEMENT_SORT_ORDER);}},
            new MenuItem{ListItemType::Button, "Sync Offline Unlocks",
            []() -> std::string {
                uint32_t count = 0;
                RA_Sync_hasPendingUnlocks(&count);
                if (count > 0) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%u pending \u2014 send to RA server", count);
                    return std::string(buf);
                }
                return std::string("No pending unlocks");
            }(),
            [](AbstractMenuItem &item) -> InputReactionHint {
                // Check authentication
                if (!CFG_getRAAuthenticated() || strlen(CFG_getRAToken()) == 0) {
                    item.setDesc("Not authenticated");
                    return NoOp;
                }

                // Check for pending unlocks
                uint32_t pending = 0;
                if (!RA_Sync_hasPendingUnlocks(&pending) || pending == 0) {
                    item.setDesc("No pending unlocks");
                    return NoOp;
                }

                // Show initial overlay with cancel hint
                char msg[128];
                snprintf(msg, sizeof(msg), "Syncing %u achievement%s...\n\n(0/%u)",
                         pending, pending == 1 ? "" : "s", pending);
                MenuList::showOverlay(msg, OverlayDismissMode::None);

                // Shared state between sync thread and main thread
                SDL_atomic_t cancel;
                SDL_AtomicSet(&cancel, 0);
                std::atomic<bool> done{false};
                std::mutex progress_mutex;
                std::string progress_msg;
                std::atomic<bool> progress_dirty{false};
                RA_SyncResult sync_result = {0, 0, 0, 0};

                // Progress callback updates shared message string
                struct ProgressCtx {
                    std::mutex* mutex;
                    std::atomic<bool>* dirty;
                    std::string* msg;
                    uint32_t total;
                };
                ProgressCtx pctx = {&progress_mutex, &progress_dirty, &progress_msg, pending};

                // Launch sync on background thread (game_id=0 for all games, NULL config for interactive defaults)
                std::thread sync_thread([&]() {
                    sync_result = RA_Sync_syncAll(0, NULL, &cancel,
                        [](uint32_t current, uint32_t total, bool success, void* userdata) {
                            auto* ctx = static_cast<ProgressCtx*>(userdata);
                            char buf[128];
                            snprintf(buf, sizeof(buf), "Syncing achievements...\n\n(%u/%u)",
                                     current, ctx->total);
                            {
                                std::lock_guard<std::mutex> lock(*ctx->mutex);
                                *ctx->msg = buf;
                            }
                            ctx->dirty->store(true);
                        }, &pctx);
                    done.store(true);
                });

                // Main thread: poll for B-button cancel and update overlay
                while (!done.load()) {
                    GFX_startFrame();
                    PAD_poll();

                    if (PAD_justPressed(BTN_B)) {
                        SDL_AtomicSet(&cancel, 1);
                        MenuList::showOverlay("Cancelling sync...", OverlayDismissMode::None);
                    }

                    // Update overlay if progress changed
                    if (progress_dirty.exchange(false)) {
                        std::string current_msg;
                        {
                            std::lock_guard<std::mutex> lock(progress_mutex);
                            current_msg = progress_msg;
                        }
                        if (!SDL_AtomicGet(&cancel)) {
                            MenuList::showOverlay(current_msg, OverlayDismissMode::None);
                        }
                    }

                    GFX_sync();
                }

                sync_thread.join();
                MenuList::hideOverlay();

                // Update button description with result
                if (SDL_AtomicGet(&cancel) && sync_result.synced == 0) {
                    item.setDesc("Sync cancelled");
                } else if (SDL_AtomicGet(&cancel) && sync_result.synced > 0) {
                    snprintf(msg, sizeof(msg), "Cancelled: %u of %u synced",
                             sync_result.synced, sync_result.total);
                    item.setDesc(msg);
                } else if (sync_result.failed > 0) {
                    snprintf(msg, sizeof(msg), "Incomplete: %u synced, retry later",
                             sync_result.synced);
                    item.setDesc(msg);
                } else if (sync_result.synced > 0) {
                    snprintf(msg, sizeof(msg), "Synced %u achievement%s",
                             sync_result.synced, sync_result.synced == 1 ? "" : "s");
                    item.setDesc(msg);
                } else {
                    item.setDesc("No pending unlocks");
                }

                return NoOp;
            }},
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu},
        });

        auto minarchMenu = new MenuList(MenuItemType::List, "In-Game",
        {
            new MenuItem{ListItemType::Generic, "Notifications", "Save state notifications", {}, {}, nullptr, nullptr, DeferToSubmenu, notificationsMenu},
            new MenuItem{ListItemType::Generic, "RetroAchievements", "Achievement tracking settings", {}, {}, nullptr, nullptr, DeferToSubmenu, retroAchievementsMenu},
        });

        // We need to alert the user about potential issues if the
        // stock OS was modified in way that are known to cause issues
        std::string bbver = extractBusyBoxVersion(execCommand("cat --help"));
        if (bbver.empty())
            bbver = "BusyBox version not found.";
        else if(deviceInfo.getPlatform() == DeviceInfo::tg5040 && bbver.find(BUSYBOX_STOCK_VERSION) == std::string::npos)
            ctx.menu->showOverlay(
                "Stock OS changes detected.\n"
                "This may cause instability or issues.\n"
                "If you experience problems, please consider\n"
                "reverting to clean stock firmware.",
                OverlayDismissMode::DismissOnA);

        auto aboutMenu = new MenuList(MenuItemType::Fixed, "About",
        {
            new StaticMenuItem{ListItemType::Generic, "Build version", "",
            []() -> std::any {
                std::ifstream t(ROOT_SYSTEM_PATH "/version.txt");
                std::stringstream buffer;
                buffer << t.rdbuf();
                return buffer.str();
            }},
            new StaticMenuItem{ListItemType::Generic, "Platform", "",
            []() -> std::any {
                return std::string(PLAT_getModel()); }
            },
            new StaticMenuItem{ListItemType::Generic, "Stock OS version", "",
            []() -> std::any {
                char osver[128];
                PLAT_getOsVersionInfo(osver, 128);
                return std::string(osver); }
            },
            new StaticMenuItem{ListItemType::Generic, "Busybox version", "",
            [&]() -> std::any { return bbver; }
            },
        });

        MenuList *buttonMenu = buildFnButtonMenu(); // nullptr if this device has none

        std::vector<AbstractMenuItem*> mainItems = {
            new MenuItem{ListItemType::Generic, "Appearance", "UI customization", {}, {}, nullptr, nullptr, DeferToSubmenu, appearanceMenu},
            new MenuItem{ListItemType::Generic, "Display", "", {}, {}, nullptr, nullptr, DeferToSubmenu, displayMenu},
            new MenuItem{ListItemType::Generic, "System", "", {}, {}, nullptr, nullptr, DeferToSubmenu, systemMenu},
        };

        if(deviceInfo.hasMuteToggle())
            mainItems.push_back(new MenuItem{ListItemType::Generic, "FN switch", "FN switch settings", {}, {}, nullptr, nullptr, DeferToSubmenu,
                new MenuList(MenuItemType::Fixed, "FN Switch", muteItems)});

        if(buttonMenu)
            mainItems.push_back(new MenuItem{ListItemType::Generic, "Assignments", "Customize button assignments", {}, {}, nullptr, nullptr, DeferToSubmenu, buttonMenu});

        mainItems.push_back(new MenuItem{ListItemType::Generic, "In-Game", "MinArch save-state notifications and achievements", {}, {}, nullptr, nullptr, DeferToSubmenu, minarchMenu});

        if(deviceInfo.hasWifi())
            mainItems.push_back(new MenuItem{ListItemType::Generic, "Network", "", {}, {}, nullptr, nullptr, DeferToSubmenu, new Wifi::Menu(appQuit, ctx.dirty)});

        if(deviceInfo.hasBluetooth())
            mainItems.push_back(new MenuItem{ListItemType::Generic, "Bluetooth", "", {}, {}, nullptr, nullptr, DeferToSubmenu, new Bluetooth::Menu(appQuit, ctx.dirty)});

        mainItems.push_back(new MenuItem{ListItemType::Generic, "About", "", {}, {}, nullptr, nullptr, DeferToSubmenu, aboutMenu});

        ctx.menu = new MenuList(MenuItemType::List, "Nocturne", mainItems);

        // Settings intentionally use a solid semantic backdrop. Arbitrary light
        // wallpaper must not turn the settings app into a light theme.
        SDL_Surface* bgbmp = NULL;

        // main content (list)
        // PADDING all around
        SDL_Rect listRect = {SCALE1(PADDING), SCALE1(PADDING), ctx.screen->w - SCALE1(PADDING * 2), ctx.screen->h - SCALE1(PADDING * 2)};
        SDL_Rect titleRect = {0, 0, 0, 0};
        // PILL_SIZE above (if showing title)
        if (ctx.appManagesTitle || ctx.appManagesIndicator)
            listRect = dy(listRect, SCALE1(PILL_SIZE));
        // BUTTON_SIZE below (if showing hints)
        if (ctx.appManagesHints)
            listRect.h -= SCALE1(BUTTON_SIZE);
        ctx.menu->performLayout(listRect);

        while (!appQuit)
        {
            GFX_startFrame();
            PAD_poll();

            if (!ctx.show_setting && BTN_MOD_SWITCHER != BTN_NONE &&
                PAD_isPressed(BTN_MENU) && PAD_justPressed(BTN_MOD_SWITCHER) &&
                !PAD_isPressed(BTN_PLUS) && !PAD_isPressed(BTN_MINUS)) {
                PAD_cancelTap(BTN_MENU);
                char persist_path[] = GAME_SWITCHER_PERSIST_PATH;
                char persist_value[] = "unused";
                putFile(persist_path, persist_value);
                appQuit = true;
                ctx.dirty = 1;
            } else {
                ctx.menu->handleInput(ctx.dirty, appQuit);
            }

            PWR_update(&ctx.dirty, &ctx.show_setting, nullptr, nullptr);

            int is_online = PWR_isOnline();
            if (was_online!=is_online)
                ctx.dirty = 1;
            was_online = is_online;

            int has_bt = PLAT_btIsConnected();
            if (had_bt != has_bt)
                ctx.dirty = 1;
            had_bt = has_bt;

            if (ctx.dirty)
            {
                GFX_clear(ctx.screen);
                if(bgbmp) {
                    SDL_Rect image_rect = {0, 0, ctx.screen->w, ctx.screen->h};
                    SDL_BlitSurface(bgbmp, NULL, ctx.screen, &image_rect);
                } else {
                    uint32_t bgc = CFG_getColor(COLOR_BACKGROUND);
                    SDL_FillRect(ctx.screen, NULL, SDL_MapRGBA(ctx.screen->format, (bgc >> 24) & 0xFF, (bgc >> 16) & 0xFF, (bgc >> 8) & 0xFF, bgc & 0xFF));
                }

                int ow = 0;

                // indicator area top right
                if (ctx.appManagesIndicator)
                {
                    ow = GFX_blitHardwareGroup(ctx.screen, ctx.show_setting);
                }
                int max_width = ctx.screen->w - SCALE1(PADDING * 2) - ow;

                // title pill
                if (ctx.appManagesTitle)
                {
                    char display_name[256];
                    int text_width = GFX_truncateText(font.large, "Some title", display_name, sizeof(display_name), max_width, SCALE1(BUTTON_PADDING * 2));
                    max_width = MIN(max_width, text_width);

                    SDL_Surface *text;
                    text = TTF_RenderUTF8_Blended(font.large, display_name, uintToColour(THEME_COLOR6_255));
                    SDL_Rect target = {SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)};
                    GFX_blitPillLight(ASSET_WHITE_PILL, ctx.screen, &target);
                    SDL_BlitSurfaceCPP(text, {0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, ctx.screen, {SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                    SDL_FreeSurface(text);
                }
                else {
                    // just set the titleRect and we will pass it on to the list to populate as needed
                    titleRect = {SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)};
                }

                // bottom area, button hints
                if (ctx.appManagesHints)
                {
                    if (ctx.show_setting && !GetHDMI())
                        GFX_blitHardwareHints(ctx.screen, ctx.show_setting);
                    else
                    {
                        char *hints[] = {(char *)("MENU"), (char *)("SLEEP"), NULL};
                        GFX_blitButtonGroup(hints, 0, ctx.screen, 0);
                    }
                    char *hints[] = {(char *)("B"), (char *)("BACK"), (char *)("A"), (char *)("OKAY"), NULL};
                    GFX_blitButtonGroup(hints, 1, ctx.screen, 1);
                }

                ctx.menu->draw(ctx.screen, listRect, titleRect);

                // present
                GFX_flip(ctx.screen);
                ctx.dirty = false;
            }
            else
                GFX_sync();

            // exit so the relauncher can re-init video at the new output
            // resolution (no-op on platforms whose GetHDMI() is stubbed)
            if (GFX_hdmiChanged())
                appQuit = true;
        }

        delete ctx.menu;
        delete appearanceMenu;
        delete systemMenu;
        ctx.menu = NULL;

        // Color pickers are owned by unique_ptrs above; destroyed automatically here.

        QuitSettings();
        PWR_quit();
        PAD_quit();
        BT_quit();
        GFX_quit();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        LOG_error("%s", e.what());
        QuitSettings();
        PWR_quit();
        PAD_quit();
        BT_quit();
        GFX_quit();

        return EXIT_FAILURE;
    }
}
