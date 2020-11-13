#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <jml.h>


static PyObject *
jml_py_version(PyObject *self)
{
    return Py_BuildValue("s", JML_VERSION_STRING);
}


static PyObject *
jml_py_platform(PyObject *self)
{
    return Py_BuildValue("s", JML_PLATFORM_STRING);
}


static PyMethodDef jml_py_methods[] = {
    {"version",     jml_py_version,     METH_NOARGS,    "Returns the version of the jml interpreter."},
    {"platform",    jml_py_platform,    METH_NOARGS,    "Returns the platform detected by jml."},
    {NULL, NULL, 0, NULL}
};


typedef struct {
    PyObject_HEAD
    jml_vm_t                       *internal_vm;
} jml_py_vm;


static void
jml_py_vm_dealloc(jml_py_vm *self)
{
    jml_vm_free(self->internal_vm);

    PyObject_GC_UnTrack(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject *
jml_py_vm_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static jml_vm_t *singleton_vm;
    if (singleton_vm == NULL)
        singleton_vm = jml_vm_new();

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
jml_py_vm_interpret(PyObject *self, PyObject *args)
{
    char *source = NULL;

    if(!PyArg_ParseTuple(args, "s", &source)) {
        return NULL;
    }

    if (jml_vm_interpret(source) == INTERPRET_OK)
        return Py_True;
    
    return Py_False;
}


static PyMethodDef jml_py_vm_methods[] = {
    {"interpret",   jml_py_vm_interpret, METH_VARARGS,  "Interprets the given source code.\n" 
                                                        "Returns True if successful, otherwise False."},
    {NULL}
};


static PyTypeObject jml_py_vm_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jml.VM",
    .tp_doc = "VM object",
    .tp_basicsize = sizeof(jml_py_vm),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_new = jml_py_vm_new,
    .tp_dealloc = (destructor) jml_py_vm_dealloc,
    .tp_methods = jml_py_vm_methods,
};


static struct PyModuleDef jml_py_module = {
    PyModuleDef_HEAD_INIT,
    "jml",
    "Python interface for the jml C interpreter.",
    -1,
    jml_py_methods
};


PyMODINIT_FUNC
PyInit_jml(void)
{
    PyObject *module;
    if (PyType_Ready(&jml_py_vm_t) < 0)
        return NULL;

    module = PyModule_Create(&jml_py_module);
    if (module == NULL) return NULL;

    Py_INCREF(&jml_py_vm_t);
    if (PyModule_AddObject(module, "VM", (PyObject*)&jml_py_vm_t) < 0) {
        Py_DECREF(&jml_py_vm_t);
        Py_DECREF(module);
        return NULL;
    }

    return module;
}
