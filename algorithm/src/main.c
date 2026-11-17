#include "engine.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static char* read_stdin_all(void) {
    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, stdin)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char* tmp = (char*)realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char** argv) {
    const char* data_dir = (argc > 1) ? argv[1] : "data";

    Engine engine;
    engine_init_struct(&engine);

    char err[512];
    if (!engine_load(&engine, data_dir, err, sizeof(err))) {
        cJSON* e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "status", "error");
        char msg[600];
        snprintf(msg, sizeof(msg), "engine initialisation failed: %s", err);
        cJSON_AddStringToObject(e, "message", msg);
        char* out = cJSON_PrintUnformatted(e);
        printf("%s\n", out);
        free(out);
        cJSON_Delete(e);
        engine_free(&engine);
        return 1;
    }

    char* input = read_stdin_all();
    if (!input) {
        engine_free(&engine);
        return 1;
    }

    cJSON* req = cJSON_Parse(input);
    free(input);
    if (!req) {
        cJSON* e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "status", "error");
        cJSON_AddStringToObject(e, "message", "invalid request JSON");
        char* out = cJSON_PrintUnformatted(e);
        printf("%s\n", out);
        free(out);
        cJSON_Delete(e);
        engine_free(&engine);
        return 1;
    }

    char* resp = engine_handle_request(&engine, req);
    printf("%s\n", resp);

    free(resp);
    cJSON_Delete(req);
    engine_free(&engine);
    return 0;
}