#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <jml.h>


static PyObject *jml_py_error_compile;

static PyObject *jml_py_error_runtime;


static PyObject *
jml_py_version(PyObject *self, PyObject *Py_UNUSED(args))
{
    return Py_BuildValue("iii", JML_VERSION_MAJOR, JML_VERSION_MINOR, JML_VERSION_MICRO);
}


static PyMethodDef jml_py_methods[] = {
    {"version",     &jml_py_version,        METH_NOARGS,    "Returns a jml version tuple containing major, minor and micro."},
    {NULL,          NULL,                   0,              NULL}
};


typedef struct {
    PyObject_HEAD
    jml_vm_t                       *internal_vm;
} jml_py_vm;


static void
jml_py_vm_dealloc(jml_py_vm *self)
{
    PyObject_GC_UnTrack(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject *
jml_py_vm_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    /*FIXME*/
    static jml_vm_t *singleton_vm;

    if (singleton_vm == NULL) {
        singleton_vm = jml_vm_new();
    }

    jml_py_vm *self;
    self = (jml_py_vm*)type->tp_alloc(type, 0);

    if (self != NULL) {
        self->internal_vm = singleton_vm;
        if (self->internal_vm == NULL) {
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject*)self;
}


static PyObject *
jml_py_vm_eval(PyObject *self, PyObject *args)
{
    char *source = NULL;

    if (!PyArg_ParseTuple(args, "s", &source))
        return NULL;

    jml_value_t value = jml_vm_eval(((jml_py_vm*)self)->internal_vm, source);

    if (!jml_value_sentinel(value)) {
        if (IS_NUM(value))
            return PyFloat_FromDouble(AS_NUM(value));

        if (IS_BOOL(value))
            return AS_BOOL(value) ? Py_True : Py_False;

        if (IS_OBJ(value)) {
            char *repr = jml_value_stringify(value);
            return _PyUnicode_FromASCII(repr, strlen(repr));
        }

        return Py_None;
    }

    PyErr_SetString(PyExc_Exception, "Source evaluation failed.");
    return NULL;
}


static PyObject *
jml_py_vm_interpret(PyObject *self, PyObject *args)
{
    char *source;

    if (!PyArg_ParseTuple(args, "s", &source))
        return NULL;

    switch (jml_vm_interpret(((jml_py_vm*)self)->internal_vm, source)) {
        case INTERPRET_OK:
            return Py_None;

        case INTERPRET_COMPILE_ERROR:
            PyErr_SetString(jml_py_error_compile, "Source compilation failed.");
            break;

        case INTERPRET_RUNTIME_ERROR:
            PyErr_SetString(jml_py_error_runtime, "Source execution failed.");
            break;
    }

    return NULL;
}


static PyMethodDef jml_py_vm_methods[] = {
    {"eval",        &jml_py_vm_eval,        METH_VARARGS,   "Evaluates the given source code."},
    {"interpret",   &jml_py_vm_interpret,   METH_VARARGS,   "Interprets the given source code.\n"
                                                            "Raises CompileError if compilation fails or RuntimeError if interpretetion fails.\n"
                                                            "Returns None on succes."},
    {NULL,          NULL,                   0,              NULL}
};


static PyTypeObject jml_py_vm_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "jml.VM",
    .tp_doc         = "VM object",
    .tp_basicsize   = sizeof(jml_py_vm),
    .tp_itemsize    = 0,
    .tp_flags       = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_new         = jml_py_vm_new,
    .tp_dealloc     = (destructor) jml_py_vm_dealloc,
    .tp_methods     = jml_py_vm_methods
};


static struct PyModuleDef jml_py_module = {
    PyModuleDef_HEAD_INIT,
    .m_name     = "jml",
    .m_doc      = "Python interface for the jml C interpreter.",
    .m_size     = -1,
    .m_methods  = jml_py_methods
};


PyMODINIT_FUNC
PyInit_jml(void)
{
    if (PyType_Ready(&jml_py_vm_t) < 0)
        return NULL;

    PyObject *mod = PyModule_Create(&jml_py_module);
    if (mod == NULL)
        return NULL;

    Py_INCREF(&jml_py_vm_t);

    if (PyModule_AddObject(mod, "VM", (PyObject*)&jml_py_vm_t) < 0) {
        Py_DECREF(&jml_py_vm_t);
        Py_DECREF(mod);
        return NULL;
    }

    jml_py_error_compile = PyErr_NewException("jml.CompileError", NULL, NULL);
    Py_XINCREF(jml_py_error_compile);

    if (PyModule_AddObject(mod, "error", jml_py_error_compile) < 0) {
        Py_XDECREF(jml_py_error_compile);
        Py_CLEAR(jml_py_error_compile);
        Py_DECREF(mod);
        return NULL;
    }

    jml_py_error_runtime = PyErr_NewException("jml.RuntimeError", NULL, NULL);
    Py_XINCREF(jml_py_error_runtime);

    if (PyModule_AddObject(mod, "error", jml_py_error_runtime) < 0) {
        Py_XDECREF(jml_py_error_runtime);
        Py_CLEAR(jml_py_error_runtime);
        Py_DECREF(mod);
        return NULL;
    }

    PyModule_AddStringConstant(mod, "VERSION",  JML_VERSION_STRING);
    PyModule_AddStringConstant(mod, "PLATFORM", JML_PLATFORM_STRING);

    return mod;
}
