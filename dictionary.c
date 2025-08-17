// Flipper Zero — English Dictionary App (Main Menu with Header & Back Exit)
// Uses Submenu's built-in header, exits on Back key.

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>

#define APP_NAME "Dictionary"
#define VERSION  "Beta"

typedef enum {
    DictionaryViewMainMenu = 0,
} DictionaryViewId;

typedef enum {
    DictionaryMenuSearch = 0,
    DictionaryMenuRandom,
    DictionaryMenuHistory,
    DictionaryMenuSettings,
    DictionaryMenuAbout,
} DictionaryMenuId;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* submenu;
} DictionaryApp;

static void dictionary_menu_cb(void* context, uint32_t index) {
    (void)context;
    switch(index) {
    case DictionaryMenuSearch:
        FURI_LOG_I(APP_NAME, "Open: Search");
        break;
    case DictionaryMenuRandom:
        FURI_LOG_I(APP_NAME, "Open: Random");
        break;
    case DictionaryMenuHistory:
        FURI_LOG_I(APP_NAME, "Open: History");
        break;
    case DictionaryMenuSettings:
        FURI_LOG_I(APP_NAME, "Open: Settings");
        break;
    case DictionaryMenuAbout:
        FURI_LOG_I(APP_NAME, "Open: About");
        break;
    default:
        break;
    }
}

// Custom input callback to handle Back button
static bool dictionary_input_callback(InputEvent* event, void* context) {
    DictionaryApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        // Stop dispatcher loop to exit app
        view_dispatcher_stop(app->vd);
        return true;
    }
    // Forward other keys to submenu
    return submenu_input(app->submenu, event);
}

static DictionaryApp* dictionary_app_alloc(void) {
    DictionaryApp* app = malloc(sizeof(DictionaryApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    app->submenu = submenu_alloc();

    // Setup submenu header and items
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "EngDict " VERSION);
    submenu_add_item(app->submenu, "Search", DictionaryMenuSearch, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "Random", DictionaryMenuRandom, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "History", DictionaryMenuHistory, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "Settings", DictionaryMenuSettings, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "About", DictionaryMenuAbout, dictionary_menu_cb, app);

    // Attach submenu view directly, with custom input handler
    View* submenu_view = submenu_get_view(app->submenu);
    view_set_input_callback(submenu_view, dictionary_input_callback);
    view_set_context(submenu_view, app);

    view_dispatcher_add_view(app->vd, DictionaryViewMainMenu, submenu_view);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void dictionary_app_free(DictionaryApp* app) {
    if(!app) return;
    view_dispatcher_remove_view(app->vd, DictionaryViewMainMenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dictionary_app_main(void* p) {
    UNUSED(p);
    DictionaryApp* app = dictionary_app_alloc();

    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
    view_dispatcher_run(app->vd);

    dictionary_app_free(app);
    return 0;
}