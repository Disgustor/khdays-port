/* The mods' native plugins (runtime/plugins): real libraries -- the SDK's Hello World and the test plugins
 * (tests/plugins/test_plugins.c, core_plugins.c) -- put in made-up mods, found, checked, loaded, started, using the
 * universal core (a service, events, hooks, the engine by name), stopped on the game's thread when another thread
 * asks, released. One mode per run (the manager keeps its plugins for the process):
 *   off   native code off: nothing loaded
 *   safe  native code on, safe mode: nothing loaded
 *   on    native code on: each case's state, the order of starts and stops, the stop asked from another thread */
#include "../runtime/plugins/plugins.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the dispatcher's entries the game's hook points call (runtime/plugins/interfaces.c) */
int khdays_hook_on(int point);
int khdays_hook_before(int point, unsigned long long *args, unsigned long long *result);
void khdays_hook_after(int point, unsigned long long *args, unsigned long long *result);

/* The engine's side for kh.engine (runtime/plugins/engine.c): a made-up memory, one routine, one datum. */
typedef struct {
    const char *name;
    void (*native)(void);
    int module;
} KhdaysRoutineName;
typedef struct {
    const char *name;
    unsigned reference;
} KhdaysDatumName;
static int test_routine(void)
{
    return 42;
}
const KhdaysRoutineName khdays_routine_names[] = {{"TestRoutine", (void (*)(void))test_routine, 22}};
const unsigned khdays_routine_name_count = 1;
const KhdaysDatumName khdays_datum_names[] = {{"gTestDatum", 0x02000010u}};
const unsigned khdays_datum_name_count = 1;
static int fake_ram[16];
unsigned char *khdays_window;
int khdays_overlay_loaded(int id)
{
    return id == 22;
}
unsigned khdays_events_vblank_count(void)
{
    return 0;
}

static int failures;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

/* every line said, "<mod id>: <text>\n" one after another */
static char said[16384];

static void heard(const char *mod_id, const char *text)
{
    const size_t n = strlen(said);
    snprintf(said + n, sizeof(said) - n, "%s: %s\n", mod_id, text);
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static void copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
    char block[65536];
    size_t n;
    while (in != NULL && out != NULL && (n = fread(block, 1, sizeof(block), in)) > 0) {
        fwrite(block, 1, n, out);
    }
    if (in != NULL) {
        fclose(in);
    }
    if (out != NULL) {
        fclose(out);
    }
}

/* A made-up mod: its folder, its mod.ini (`ini`, with %s for the platform's key) and its library (NULL none). */
static void mod(const char *folder, const char *ini, const char *library)
{
    char path[512], text[1024];
    snprintf(path, sizeof(path), "plugins_test_mods/%s", folder);
    SDL_CreateDirectory(path);
    snprintf(path, sizeof(path), "plugins_test_mods/%s/native", folder);
    SDL_CreateDirectory(path);
    snprintf(text, sizeof(text), ini, khdays_plugins_platform());
    snprintf(path, sizeof(path), "plugins_test_mods/%s/mod.ini", folder);
    write_file(path, text);
    if (library != NULL) {
        snprintf(path, sizeof(path), "plugins_test_mods/%s/native/plugin.dll", folder);
        copy_file(library, path);
    }
}

static const KhdaysPluginView *by_id(const char *id)
{
    for (int i = 0; i < khdays_plugins_count(); ++i) {
        if (strcmp(khdays_plugins_at(i)->mod_id, id) == 0) {
            return khdays_plugins_at(i);
        }
    }
    return NULL;
}

static int state(const char *id)
{
    const KhdaysPluginView *v = by_id(id);
    return v != NULL ? v->state : -1;
}

static int why_has(const char *id, const char *part)
{
    const KhdaysPluginView *v = by_id(id);
    return v != NULL && strstr(v->why, part) != NULL;
}

static void make_mods(void)
{
    SDL_CreateDirectory("plugins_test_mods");
    remove("plugin_stopped.txt");
    mod("A hello",
        "id = example.hello-world\nname = Hello\nversion = 1.0\nauthor = Tester\nsdk = 1\n"
        "native.%s = native/plugin.dll\n",
        KHDAYS_TEST_HELLO);
    mod("B other sdk", "id = test.other-sdk\nname = B\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\n",
        KHDAYS_TEST_OTHER_SDK);
    mod("C failing", "id = test.failing\nname = C\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\n",
        KHDAYS_TEST_FAILING);
    mod("D counting",
        "id = test.counting\nname = D\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\n"
        "requires = example.hello-world, test.data-only\n",
        KHDAYS_TEST_COUNTING);
    mod("E missing", "id = test.missing\nname = E\nversion = 1\nsdk = 1\nnative.%s = native/none.dll\n", NULL);
    mod("F sdk 9", "id = test.sdk9\nname = F\nversion = 1\nsdk = 9\nnative.%s = native/plugin.dll\n",
        KHDAYS_TEST_HELLO);
    mod("G requires",
        "id = test.requires\nname = G\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\n"
        "requires = nobody.here\n",
        KHDAYS_TEST_HELLO);
    mod("H outside", "id = test.outside\nname = H\nversion = 1\nsdk = 1\nnative.%s = ../A hello/native/plugin.dll\n",
        NULL);
    mod("I data only", "id = test.data-only\nname = I\nversion = 1\n# a mod%s without a plugin\n", NULL);
    mod("J provider", "id = test.provider\nname = J\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\n",
        KHDAYS_TEST_PROVIDER);
    mod("K user",
        "id = test.user\nname = K\nversion = 1\nsdk = 1\nnative.%s = native/plugin.dll\nrequires = test.provider\n",
        KHDAYS_TEST_USER);
}

static int SDLCALL quit_thread(void *unused)
{
    (void)unused;
    khdays_plugins_quit(5000);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "on";
    fake_ram[4] = 1234;                                      /* gTestDatum, at 0x02000010 */
    khdays_window = (unsigned char *)fake_ram - 0x02000000u; /* references from main RAM's start */
    make_mods();
    khdays_plugins_set_native_code(strcmp(mode, "off") != 0);
    khdays_plugins_set_safe_mode(strcmp(mode, "safe") == 0);
    khdays_plugins_start("plugins_test_mods", heard);
    CHECK(khdays_plugins_count() == 10); /* I has no plugin */
    if (strcmp(mode, "on") != 0) {
        CHECK(state("example.hello-world") == KHDAYS_PLUGIN_WAITING);
        CHECK(state("test.counting") == KHDAYS_PLUGIN_WAITING);
        CHECK(strstr(said, strcmp(mode, "safe") == 0 ? "safe mode" : "native code is off") != NULL);
        CHECK(strstr(said, "Hello from") == NULL);
        CHECK(state("test.sdk9") == KHDAYS_PLUGIN_REFUSED); /* told before any loading */
    } else {
        CHECK(state("example.hello-world") == KHDAYS_PLUGIN_STARTED);
        CHECK(strstr(said, "example.hello-world: plugin Hello from a native plugin") != NULL);
        CHECK(strstr(said, "by Tester") != NULL);
        CHECK(state("test.other-sdk") == KHDAYS_PLUGIN_REFUSED && why_has("test.other-sdk", "built for SDK 2.0"));
        CHECK(state("test.failing") == KHDAYS_PLUGIN_FAILED && why_has("test.failing", "start failed"));
        CHECK(strstr(said, "test.failing: plugin error: start fails on purpose") != NULL);
        CHECK(state("test.counting") == KHDAYS_PLUGIN_STARTED); /* after the mod it requires */
        CHECK(strstr(said, "Hello from") < strstr(said, "test.counting: plugin counting"));
        CHECK(state("test.missing") == KHDAYS_PLUGIN_REFUSED && why_has("test.missing", "is not there"));
        CHECK(state("test.sdk9") == KHDAYS_PLUGIN_REFUSED && why_has("test.sdk9", "built for SDK 9"));
        CHECK(state("test.requires") == KHDAYS_PLUGIN_REFUSED && why_has("test.requires", "nobody.here"));
        CHECK(state("test.outside") == KHDAYS_PLUGIN_REFUSED && why_has("test.outside", "not a file inside"));

        /* the universal core, between test.provider and test.user */
        CHECK(state("test.provider") == KHDAYS_PLUGIN_STARTED && state("test.user") == KHDAYS_PLUGIN_STARTED);
        CHECK(strstr(said, "test.provider: plugin no such point refused") != NULL);
        CHECK(strstr(said, "test.provider: hook not added: no hook point NoSuchPoint") != NULL);
        CHECK(strstr(said, "test.user: plugin math 5") != NULL);
        CHECK(strstr(said, "test.user: plugin no math 2") != NULL);
        CHECK(strstr(said, "test.provider: plugin test.user.ping hello") != NULL);
        CHECK(strstr(said, "test.user: plugin frame refused") != NULL);
        CHECK(strstr(said, "replaces it as test.provider's does") != NULL);
        CHECK(strstr(said, "routine 42 in module 22, datum 1234, none 0000000000000000, back same") != NULL);
        khdays_plugins_game_event("frame", 7, 0, 0, 0, NULL);
        CHECK(strstr(said, "test.provider: plugin frame 7") != NULL);
        {
            /* as a hooked routine's wrapper does: TestPoint(a, b) = a + b */
            unsigned long long args[2] = {5, 10}, result = 0;
            CHECK(khdays_hook_on(0));
            CHECK(!khdays_hook_before(0, args, &result) && args[1] == 20); /* doubled by the user, first */
            result = args[0] + args[1];
            khdays_hook_after(0, args, &result);
            CHECK(result == 26); /* the provider's after-function */
            args[0] = 99;
            args[1] = 1;
            result = 0;
            CHECK(khdays_hook_before(0, args, &result) && result == 7 && args[1] == 2); /* skipped by the provider */
            khdays_hook_after(0, args, &result);
            CHECK(result == 8);
        }

        /* the window's thread asks; this, the game's, stops them at its next frame */
        SDL_Thread *thread = SDL_CreateThread(quit_thread, "quit", NULL);
        for (int frame = 0; frame < 500 && state("test.counting") == KHDAYS_PLUGIN_STARTED; ++frame) {
            SDL_Delay(10);
            khdays_plugins_frame();
        }
        SDL_WaitThread(thread, NULL);
        CHECK(state("example.hello-world") == KHDAYS_PLUGIN_STOPPED);
        CHECK(state("test.counting") == KHDAYS_PLUGIN_STOPPED);
        /* the last started first */
        CHECK(strstr(said, "test.counting: plugin stopping") != NULL &&
              strstr(said, "test.counting: plugin stopping") < strstr(said, "Goodbye"));
        {
            FILE *f = fopen("plugin_stopped.txt", "rb");
            char text[32] = "";
            if (f != NULL) {
                fgets(text, sizeof(text), f);
                fclose(f);
            }
            CHECK(strcmp(text, "stopped once") == 0);
        }
        CHECK(!khdays_hook_on(0) && !khdays_plugins_listening()); /* their hooks and subscriptions gone */
        khdays_plugins_stop();                                    /* again: nothing left to stop */
        CHECK(strstr(strstr(said, "Goodbye") + 1, "Goodbye") == NULL);
    }
    if (failures != 0) {
        fprintf(stderr, "what was said:\n%s", said);
    } else {
        printf("plugins (%s): all checks passed\n", mode);
    }
    return failures != 0;
}
