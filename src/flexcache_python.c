// src/flexcache_python.c


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <datetime.h>
#include <string.h>
#include <stdlib.h>

#include "flexcache.h"
#include "flexcache_policy_lru.h"
#include "flexcache_policy_fifo.h"
#include "flexcache_policy_random.h"

/* ============================================================
 *  Python wrapper object
 * ============================================================ */

typedef struct {
    PyObject_HEAD
    flexcache cache;
    flexcache_random_policy *random_policy;
    int policy_type;  /* 0=lru, 1=fifo, 2=random */
} PyFlexCache;

/* ============================================================
 *  Callbacks
 * ============================================================ */

static void *
py_key_copy(const void *ptr, size_t len, void *user_ctx)
{
    (void)user_ctx;
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, ptr, len);
    copy[len] = '\0';
    return copy;
}

static void
py_key_free(void *ptr, void *user_ctx)
{
    (void)user_ctx;
    free(ptr);
}

static void *
py_value_copy(const void *ptr, size_t len, void *user_ctx)
{
    (void)len;
    (void)user_ctx;
    PyObject *obj = (PyObject *)ptr;
    Py_INCREF(obj);
    return obj;
}

static void
py_value_free(void *ptr, void *user_ctx)
{
    (void)user_ctx;
    PyObject *obj = (PyObject *)ptr;
    Py_DECREF(obj);
}

static void
py_ondelete(void *key, size_t key_len, void *value, int64_t byte_size, void *user_ctx)
{
    (void)key;
    (void)key_len;
    (void)byte_size;
    (void)user_ctx;
    
    PyObject *obj = (PyObject *)value;
    if (!obj) return;
    
    if (PyObject_HasAttrString(obj, "close")) {
        PyObject *result = PyObject_CallMethod(obj, "close", NULL);
        if (result) {
            Py_DECREF(result);
        } else {
            PyErr_Clear();
        }
    }
}

static int64_t
py_get_byte_size(PyObject *obj)
{
    if (PyObject_HasAttrString(obj, "item_size")) {
        PyObject *result = PyObject_CallMethod(obj, "item_size", NULL);
        if (result) {
            int64_t size = PyLong_AsLongLong(result);
            Py_DECREF(result);
            if (!PyErr_Occurred()) {
                return size;
            }
            PyErr_Clear();
        } else {
            PyErr_Clear();
        }
    }
    return 1;
}

static uint64_t
py_now_ms(void *user_ctx)
{
    (void)user_ctx;
    
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

static uint32_t
py_rng(void *rng_ctx)
{
    (void)rng_ctx;
    return (uint32_t)rand();
}

/* ============================================================
 *  Type methods
 * ============================================================ */

static void
PyFlexCache_dealloc(PyFlexCache *self)
{
    flexcache_destroy(&self->cache);
    
    if (self->random_policy) {
        flexcache_policy_random_destroy(self->random_policy);
    }
    
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
PyFlexCache_init(PyFlexCache *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"eviction_policy", "scan_interval", "max_items", "max_bytes", NULL};
    
    const char *policy_str = "lru";
    double scan_interval_sec = 0.0;
    Py_ssize_t max_items = 0;
    Py_ssize_t max_bytes = 0;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sdnn", kwlist,
            &policy_str, &scan_interval_sec, &max_items, &max_bytes)) {
        return -1;
    }
    
    uint64_t scan_interval_ms = (uint64_t)(scan_interval_sec * 1000.0);
    
    int rc = flexcache_init(
        &self->cache,
        py_now_ms,
        (size_t)max_items,
        (int64_t)max_bytes,
        scan_interval_ms,
        py_key_copy,
        py_key_free,
        py_value_copy,
        py_value_free,
        py_ondelete,
        NULL
    );
    
    if (rc != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize cache");
        return -1;
    }
    
    self->random_policy = NULL;
    
    if (strcmp(policy_str, "lru") == 0) {
        self->policy_type = 0;
        flexcache_policy_lru_init(&self->cache);
    } else if (strcmp(policy_str, "fifo") == 0) {
        self->policy_type = 1;
        flexcache_policy_fifo_init(&self->cache);
    } else if (strcmp(policy_str, "random") == 0) {
        self->policy_type = 2;
        self->random_policy = flexcache_policy_random_create(py_rng, NULL);
        if (!self->random_policy) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create random policy");
            return -1;
        }
        flexcache_policy_random_init(&self->cache, self->random_policy);
    } else {
        PyErr_SetString(PyExc_ValueError, "eviction_policy must be 'lru', 'fifo', or 'random'");
        return -1;
    }
    
    return 0;
}

static PyObject *
PyFlexCache_set(PyFlexCache *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"key", "value", "ttl", NULL};
    
    const char *key;
    Py_ssize_t key_len;
    PyObject *value;
    PyObject *ttl_obj = Py_None;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#O|O", kwlist,
            &key, &key_len, &value, &ttl_obj)) {
        return NULL;
    }
    
    if (key_len == 0) {
        PyErr_SetString(PyExc_ValueError, "Key cannot be empty");
        return NULL;
    }
    
    uint64_t ttl_ms = 0;
    uint64_t expires_at_ms = 0;
    
    if (ttl_obj != Py_None) {
        if (PyDelta_Check(ttl_obj)) {
            /* timedelta = relative TTL */
            PyObject *total_sec = PyObject_CallMethod(ttl_obj, "total_seconds", NULL);
            if (!total_sec) return NULL;
            double sec = PyFloat_AsDouble(total_sec);
            Py_DECREF(total_sec);
            if (PyErr_Occurred()) return NULL;
            
            if (sec > 0) {
                ttl_ms = (uint64_t)(sec * 1000.0);
            }
            /* sec <= 0 means no expiration (ttl_ms stays 0) */
        }
        else if (PyDateTime_Check(ttl_obj)) {
            /* datetime = absolute expiration */
            /* Convert Python datetime to our internal timestamp */
            
            /* Get current times in both systems */
            uint64_t internal_now = py_now_ms(NULL);
            
            PyObject *datetime_mod = PyImport_ImportModule("datetime");
            if (!datetime_mod) return NULL;
            
            PyObject *datetime_cls = PyObject_GetAttrString(datetime_mod, "datetime");
            Py_DECREF(datetime_mod);
            if (!datetime_cls) return NULL;
            
            PyObject *py_now = PyObject_CallMethod(datetime_cls, "now", NULL);
            Py_DECREF(datetime_cls);
            if (!py_now) return NULL;
            
            /* Calculate delta from Python's now to target datetime */
            PyObject *delta = PyNumber_Subtract(ttl_obj, py_now);
            Py_DECREF(py_now);
            if (!delta) return NULL;
            
            PyObject *total_sec = PyObject_CallMethod(delta, "total_seconds", NULL);
            Py_DECREF(delta);
            if (!total_sec) return NULL;
            
            double sec = PyFloat_AsDouble(total_sec);
            Py_DECREF(total_sec);
            if (PyErr_Occurred()) return NULL;
            
            /* Convert to absolute internal timestamp */
            /* expires_at = internal_now + delta_ms */
            int64_t delta_ms = (int64_t)(sec * 1000.0);
            
            if (delta_ms <= 0) {
                /* Datetime in past or now = already expired */
                /* Set to 1 so it's less than any future now_ms */
                expires_at_ms = 1;
            } else {
                expires_at_ms = internal_now + (uint64_t)delta_ms;
            }
        }
        else {
            PyErr_SetString(PyExc_TypeError, "ttl must be timedelta or datetime");
            return NULL;
        }
    }
    
    int64_t byte_size = py_get_byte_size(value);
    
    int rc = flexcache_insert(
        &self->cache,
        key, (size_t)key_len,
        value, 0,
        byte_size,
        ttl_ms,
        expires_at_ms
    );
    
    if (rc == -1) {
        PyErr_SetString(PyExc_KeyError, "Key already exists");
        return NULL;
    } else if (rc == -2) {
        PyErr_SetString(PyExc_MemoryError, "Allocation failed");
        return NULL;
    }
    
    Py_RETURN_NONE;
}

static PyObject *
PyFlexCache_get(PyFlexCache *self, PyObject *args)
{
    const char *key;
    Py_ssize_t key_len;
    
    if (!PyArg_ParseTuple(args, "s#", &key, &key_len)) {
        return NULL;
    }
    
    void *value = flexcache_get(&self->cache, key, (size_t)key_len);
    
    if (!value) {
        Py_RETURN_NONE;
    }
    
    PyObject *obj = (PyObject *)value;
    Py_INCREF(obj);
    return obj;
}

static PyObject *
PyFlexCache_delete(PyFlexCache *self, PyObject *args)
{
    const char *key;
    Py_ssize_t key_len;
    
    if (!PyArg_ParseTuple(args, "s#", &key, &key_len)) {
        return NULL;
    }
    
    int rc = flexcache_delete(&self->cache, key, (size_t)key_len);
    
    if (rc == 0) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *
PyFlexCache_get_items(PyFlexCache *self, void *closure)
{
    (void)closure;
    return PyLong_FromSize_t(flexcache_item_count(&self->cache));
}

static PyObject *
PyFlexCache_get_bytes(PyFlexCache *self, void *closure)
{
    (void)closure;
    return PyLong_FromLongLong(flexcache_total_bytes(&self->cache));
}

static PyObject *
PyFlexCache_scan(PyFlexCache *self, PyObject *Py_UNUSED(args))
{
    flexcache_scan_and_clean(&self->cache);
    Py_RETURN_NONE;
}

static PyObject *
PyFlexCache_clear(PyFlexCache *self, PyObject *Py_UNUSED(args))
{
    bcache_node *n;
    bcache_node *next;
    bcache_node *head;

    head = self->cache.base.list;
    if (!head) {
        Py_RETURN_NONE;
    }

    /* Iterate and delete all nodes properly */
    while (self->cache.base.list) {
        n = self->cache.base.list;
        
        /* Call ondelete and free callbacks */
        void *key = n->key;
        void *value = ((flexcache_entry *)n->value)->user_value;
        
        /* Call close() if exists */
        if (value && PyObject_HasAttrString((PyObject *)value, "close")) {
            PyObject *result = PyObject_CallMethod((PyObject *)value, "close", NULL);
            if (result) {
                Py_DECREF(result);
            } else {
                PyErr_Clear();
            }
        }
        
        /* Decref value */
        if (value) {
            Py_DECREF((PyObject *)value);
        }
        
        /* Free entry struct */
        free(n->value);
        
        /* Free key */
        free(key);
        
        /* Remove from bcache */
        bcache_remove_node(&self->cache.base, n);
    }
    
    Py_RETURN_NONE;
}

/* ============================================================
 *  Type definition
 * ============================================================ */

static PyMethodDef PyFlexCache_methods[] = {
    {"set", (PyCFunction)PyFlexCache_set, METH_VARARGS | METH_KEYWORDS, "Set a cache entry"},
    {"get", (PyCFunction)PyFlexCache_get, METH_VARARGS, "Get a cache entry"},
    {"delete", (PyCFunction)PyFlexCache_delete, METH_VARARGS, "Delete a cache entry"},
    {"scan", (PyCFunction)PyFlexCache_scan, METH_NOARGS, "Run expiration scan"},
    {"clear", (PyCFunction)PyFlexCache_clear, METH_NOARGS, "Clear all entries"},
    {NULL}
};

static PyGetSetDef PyFlexCache_getsetters[] = {
    {"items", (getter)PyFlexCache_get_items, NULL, "Number of items", NULL},
    {"bytes", (getter)PyFlexCache_get_bytes, NULL, "Total bytes", NULL},
    {NULL}
};

static PyTypeObject PyFlexCacheType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "flexcache._flexcache.FlexCache",
    .tp_doc = "FlexCache with TTL and pluggable eviction",
    .tp_basicsize = sizeof(PyFlexCache),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)PyFlexCache_init,
    .tp_dealloc = (destructor)PyFlexCache_dealloc,
    .tp_methods = PyFlexCache_methods,
    .tp_getset = PyFlexCache_getsetters,
};

/* ============================================================
 *  Module
 * ============================================================ */

static PyModuleDef flexcache_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_flexcache",
    .m_doc = "Python bindings for flexcache",
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit__flexcache(void)
{
    PyDateTime_IMPORT;
    
    if (PyType_Ready(&PyFlexCacheType) < 0)
        return NULL;
    
    PyObject *m = PyModule_Create(&flexcache_module);
    if (!m)
        return NULL;
    
    Py_INCREF(&PyFlexCacheType);
    if (PyModule_AddObject(m, "FlexCache", (PyObject *)&PyFlexCacheType) < 0) {
        Py_DECREF(&PyFlexCacheType);
        Py_DECREF(m);
        return NULL;
    }
    
    return m;
}
