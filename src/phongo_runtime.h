#ifndef PHONGO_RUNTIME_H
#define PHONGO_RUNTIME_H
#include "php_mongodb_runtime.h"

void phongo_runtime_init(void);
void phongo_runtime_wrap_handlers(zend_class_entry *ce, zend_object_handlers *handlers);
bool phongo_runtime_acquire(const void *key, size_t length);
void phongo_runtime_release(const void *key, size_t length);
void phongo_runtime_forget(const void *key, size_t length);
#endif
