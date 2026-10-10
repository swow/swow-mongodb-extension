/* Client ownership stays in the PHP driver. Consumers never inspect its
 * private objects, and persistent Managers still share their original client. */
#include <php.h>
#include <Zend/zend_extensions.h>
#include <mongoc/mongoc.h>
#include "phongo.h"
#include "phongo_runtime.h"
#include "phongo_error.h"
#include <mongoc/mongoc-runtime.h>

static int runtime_slot;

typedef struct {
    zend_class_entry *ce;
    void (*free_object)(zend_object *);
} phongo_runtime_class;
static phongo_runtime_class runtime_classes[5];
static size_t runtime_class_count;

typedef struct {
    const void *keys[3];
    size_t count;
} phongo_runtime_guard;

ZEND_DLEXPORT bool php_mongodb_register_runtime(uint32_t abi, size_t size, const php_mongodb_runtime_hooks *hooks)
{
    if (abi != PHP_MONGODB_RUNTIME_ABI || size != sizeof(*hooks)) {
        return false;
    }
    if (hooks && (!hooks->active || !hooks->poll || !hooks->sleep || !hooks->work ||
                  !hooks->acquire || !hooks->release || !hooks->forget)) {
        return false;
    }
    mongoc_runtime_set_hooks(hooks);
    return true;
}

bool phongo_runtime_acquire(const void *key, size_t length)
{
    const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
    if (!hooks || hooks->acquire(key, length)) {
        return true;
    }
    if (!EG(exception)) {
        phongo_throw_exception(PHONGO_ERROR_RUNTIME, "MongoDB client operation was cancelled");
    }
    return false;
}

void phongo_runtime_release(const void *key, size_t length)
{
    const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
    if (hooks) {
        hooks->release(key, length);
    }
}

void phongo_runtime_forget(const void *key, size_t length)
{
    const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
    if (hooks) {
        hooks->forget(key, length);
    }
}

static void guard_add(phongo_runtime_guard *guard, const void *key)
{
    if (key) {
        for (size_t i = 0; i < guard->count; i++) {
            if (guard->keys[i] == key) {
                return;
            }
        }
        guard->keys[guard->count++] = key;
    }
}

static void guard_manager(phongo_runtime_guard *guard, phongo_manager_t *manager)
{
    guard_add(guard, manager->client);
    if (Z_TYPE(manager->key_vault_client_manager) == IS_OBJECT) {
        guard_add(guard, Z_MANAGER_OBJ_P(&manager->key_vault_client_manager)->client);
    }
}

static phongo_runtime_guard guard_for_object(zend_object *object)
{
    phongo_runtime_guard guard = {{NULL}, 0};
    zval *manager = NULL;
    if (!object || !mongoc_runtime_hooks()) {
        return guard;
    }
    if (object->ce == phongo_manager_ce) {
        guard_manager(&guard, Z_OBJ_MANAGER(object));
    } else if (object->ce == phongo_server_ce) {
        manager = &Z_OBJ_SERVER(object)->manager;
    } else if (object->ce == phongo_cursor_ce) {
        manager = &Z_OBJ_CURSOR(object)->manager;
    } else if (object->ce == phongo_session_ce) {
        manager = &Z_OBJ_SESSION(object)->manager;
    } else if (object->ce == phongo_clientencryption_ce) {
        guard_add(&guard, Z_OBJ_CLIENTENCRYPTION(object)->client_encryption);
        manager = &Z_OBJ_CLIENTENCRYPTION(object)->key_vault_client_manager;
    }
    if (manager && Z_TYPE_P(manager) == IS_OBJECT) {
        guard_manager(&guard, Z_MANAGER_OBJ_P(manager));
    }
    /* Acquire both a main and a key-vault client in a consistent order. */
    for (size_t i = 0; i < guard.count; i++) {
        for (size_t j = i + 1; j < guard.count; j++) {
            if ((uintptr_t) guard.keys[j] < (uintptr_t) guard.keys[i]) {
                const void *key = guard.keys[i];
                guard.keys[i] = guard.keys[j];
                guard.keys[j] = key;
            }
        }
    }
    return guard;
}

static void guard_release(phongo_runtime_guard *guard)
{
    while (guard->count) {
        const void *key = guard->keys[--guard->count];
        phongo_runtime_release(&key, sizeof(key));
    }
}

static bool guard_acquire(phongo_runtime_guard *guard, bool cleanup)
{
    size_t count = guard->count;
    guard->count = 0;
    while (guard->count < count) {
        const void *key = guard->keys[guard->count];
        const php_mongodb_runtime_hooks *hooks = mongoc_runtime_hooks();
        if (cleanup && hooks) {
            /* A destructor cannot use a client still owned by another
             * coroutine. Finish this wait even if cancellation interrupts it. */
            while (!hooks->acquire(&key, sizeof(key))) {
            }
        } else if (!phongo_runtime_acquire(&key, sizeof(key))) {
            guard_release(guard);
            return false;
        }
        guard->count++;
    }
    return true;
}

static void phongo_runtime_dispatch(INTERNAL_FUNCTION_PARAMETERS)
{
    zif_handler original = (zif_handler) execute_data->func->internal_function.reserved[runtime_slot];
    phongo_runtime_guard guard = guard_for_object(Z_TYPE(execute_data->This) == IS_OBJECT ? Z_OBJ(execute_data->This) : NULL);
    if (!guard_acquire(&guard, false)) {
        return;
    }
    zend_try {
        original(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    } zend_catch {
        guard_release(&guard);
        zend_bailout();
    } zend_end_try();
    guard_release(&guard);
}

static void phongo_runtime_free_object(zend_object *object)
{
    phongo_runtime_guard guard = guard_for_object(object);
    void (*original)(zend_object *) = NULL;
    for (size_t i = 0; i < runtime_class_count; i++) {
        if (runtime_classes[i].ce == object->ce) {
            original = runtime_classes[i].free_object;
            break;
        }
    }
    ZEND_ASSERT(original);
    (void) guard_acquire(&guard, true);
    zend_try {
        original(object);
    } zend_catch {
        guard_release(&guard);
        zend_bailout();
    } zend_end_try();
    guard_release(&guard);
}

void phongo_runtime_wrap_handlers(zend_class_entry *ce, zend_object_handlers *handlers)
{
    ZEND_ASSERT(runtime_class_count < sizeof(runtime_classes) / sizeof(runtime_classes[0]));
    runtime_classes[runtime_class_count++] = (phongo_runtime_class) {ce, handlers->free_obj};
    handlers->free_obj = phongo_runtime_free_object;
}

void phongo_runtime_init(void)
{
    zend_class_entry *classes[] = {phongo_manager_ce, phongo_server_ce, phongo_cursor_ce, phongo_session_ce, phongo_clientencryption_ce};
    runtime_slot = zend_get_resource_handle("mongodb native runtime");
    ZEND_ASSERT(runtime_slot >= 0);
    for (size_t i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
        zend_function *function;
        ZEND_HASH_FOREACH_PTR(&classes[i]->function_table, function) {
            if (function->type == ZEND_INTERNAL_FUNCTION && function->internal_function.handler != phongo_runtime_dispatch) {
                function->internal_function.reserved[runtime_slot] = (void *) function->internal_function.handler;
                function->internal_function.handler = phongo_runtime_dispatch;
            }
        } ZEND_HASH_FOREACH_END();
    }
}
