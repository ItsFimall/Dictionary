// Flipper Zero — English Dictionary App
// Step 1.1: Fix back button behavior and clean up code.

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>

#define APP_NAME "Dictionary"
#define VERSION  "SearchInpuGUI"

// An enumeration for the different views in our application.
typedef enum {
    DictionaryViewMainMenu = 0,
    DictionaryViewSearchInput,
} DictionaryViewId;

// An enumeration for the menu items.
typedef enum {
    DictionaryMenuSearch = 0,
    DictionaryMenuRandom,
    DictionaryMenuHistory,
    DictionaryMenuSettings,
    DictionaryMenuAbout,
} DictionaryMenuId;

// The main application structure.
typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* submenu;
    TextInput* text_input;

    char* search_buffer;
    size_t search_buffer_size;
} DictionaryApp;

// Forward declarations for our callback functions.
static void dictionary_search_done_cb(void* context);
static bool dictionary_navigation_event_callback(void* context);

// Main menu callback: handles actions when a menu item is selected.
static void dictionary_menu_cb(void* context, uint32_t index) {
    DictionaryApp* app = context;

    switch(index) {
    case DictionaryMenuSearch:
        // Clear the buffer before showing the input view.
        memset(app->search_buffer, 0, app->search_buffer_size);

        // Configure the text input view.
        text_input_set_header_text(app->text_input, "Search for a word");
        text_input_set_result_callback(
            app->text_input,
            dictionary_search_done_cb,
            app,
            app->search_buffer,
            app->search_buffer_size,
            true);

        // Switch to the text input view.
        view_dispatcher_switch_to_view(app->vd, DictionaryViewSearchInput);
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

// Callback for when text input is complete.
static void dictionary_search_done_cb(void* context) {
    DictionaryApp* app = context;

    // The user's input is now in app->search_buffer.
    FURI_LOG_I(APP_NAME, "Word to search: %s", app->search_buffer);

    // TODO: Implement search logic here.

    // For now, just return to the main menu.
    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
}

// A generic navigation event callback.
// It's called when the user presses the back button.
static bool dictionary_navigation_event_callback(void* context) {
    UNUSED(context);
    // By returning false, we tell the view dispatcher that we haven't
    // handled the event, and it should perform its default action,
    // which is to close the current view. If the current view is the last one,
    // the application will exit. This fixes the back button behavior.
    return false;
}

// Allocate and initialize the application.
static DictionaryApp* dictionary_app_alloc(void) {
    DictionaryApp* app = malloc(sizeof(DictionaryApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();

    // --- Set up View Dispatcher callbacks ---
    // This is the key change to fix the back button.
    // We set a callback that is triggered on navigation events (like back button presses).
    view_dispatcher_set_navigation_event_callback(app->vd, dictionary_navigation_event_callback);
    view_dispatcher_set_event_callback_context(app->vd, app);

    // --- Submenu (Main Menu) Setup ---
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "EngDict " VERSION);
    submenu_add_item(app->submenu, "Search", DictionaryMenuSearch, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "Random", DictionaryMenuRandom, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "History", DictionaryMenuHistory, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "Settings", DictionaryMenuSettings, dictionary_menu_cb, app);
    submenu_add_item(app->submenu, "About", DictionaryMenuAbout, dictionary_menu_cb, app);

    view_dispatcher_add_view(app->vd, DictionaryViewMainMenu, submenu_get_view(app->submenu));

    // --- TextInput (Search Input) Setup ---
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->vd, DictionaryViewSearchInput, text_input_get_view(app->text_input));

    app->search_buffer_size = 50;
    app->search_buffer = malloc(app->search_buffer_size);

    // Attach the view dispatcher to the GUI.
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

// Free all allocated resources.
static void dictionary_app_free(DictionaryApp* app) {
    if(!app) return;

    view_dispatcher_remove_view(app->vd, DictionaryViewSearchInput);
    text_input_free(app->text_input);
    free(app->search_buffer);

    view_dispatcher_remove_view(app->vd, DictionaryViewMainMenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
}

// Application entry point.
int32_t dictionary_app_main(void* p) {
    UNUSED(p);
    DictionaryApp* app = dictionary_app_alloc();

    view_dispatcher_switch_to_view(app->vd, DictionaryViewMainMenu);
    view_dispatcher_run(app->vd);

    dictionary_app_free(app);
    return 0;
}
