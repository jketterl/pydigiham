#include "mbesynthesizer.hpp"
#include "types.hpp"

#include <cstring>
#include <digiham/mbe_synthesizer.hpp>

static Digiham::Mbe::Mode* convertToAmbeMode(PyObject* mode) {
    PyTypeObject* TableModeType = getAmbeTableModeType();
    int rc = PyObject_IsInstance(mode, (PyObject*) TableModeType);
    Py_DECREF(TableModeType);

    if (rc == -1) return nullptr;
    if (rc) {
        PyObject* indexObj = PyObject_CallMethod(mode, "getIndex", NULL);
        if (indexObj == NULL) {
            return nullptr;
        }
        if (!PyLong_Check(indexObj)) {
            Py_DECREF(indexObj);
            return nullptr;
        }
        unsigned int index = PyLong_AsUnsignedLong(indexObj);
        if (PyErr_Occurred()) {
            Py_DECREF(indexObj);
            return nullptr;
        }
        Py_DECREF(indexObj);

        return new Digiham::Mbe::TableMode(index);
    }

    PyTypeObject* ControlWordModeType = getAmbeControlWordModeType();
    rc = PyObject_IsInstance(mode, (PyObject*) ControlWordModeType);
    Py_DECREF(ControlWordModeType);

    if (rc == -1) return nullptr;
    if (rc) {
        PyObject* controlWordBytes = PyObject_CallMethod(mode, "getBytes", NULL);
        if (controlWordBytes == NULL) {
            return nullptr;
        }
        if (!PyBytes_Check(controlWordBytes)) {
            Py_DECREF(controlWordBytes);
            return nullptr;
        }
        if (PyBytes_Size(controlWordBytes) != 12) {
            Py_DECREF(controlWordBytes);
            PyErr_SetString(PyExc_ValueError, "control word size mismatch, should be 12");
            return nullptr;
        }

        short* controlWords = (short*) malloc(sizeof(short) * 6);
        std::memcpy(controlWords, PyBytes_AsString(controlWordBytes), sizeof(short) * 6);

        auto result = new Digiham::Mbe::ControlWordMode(controlWords);
        free(controlWords);
        return result;
    }

    PyTypeObject* DynamicModeType = getAmbeDynamicModeType();
    rc = PyObject_IsInstance(mode, (PyObject*) DynamicModeType);
    Py_DECREF(DynamicModeType);

    if (rc == -1) return nullptr;
    if (rc) {
        Py_INCREF(mode);
        return new Digiham::Mbe::DynamicMode([mode] (unsigned char code) {
            // acquire GIL
            PyGILState_STATE gstate;
            gstate = PyGILState_Ensure();

            PyObject* newMode = PyObject_CallMethod(mode, "getModeFor", "b", code);
            Digiham::Mbe::Mode* result = nullptr;
            if (newMode == NULL) {
                std::cerr << "failed to get mode for code " << +code << "\n";
            } else if (newMode == Py_None) {
                std::cerr << "mode for code " << +code << " was None\n";
                Py_DECREF(newMode);
            } else {
                result = convertToAmbeMode(newMode);
                Py_DECREF(newMode);
            }

            /* Release the thread. No Python API allowed beyond this point. */
            PyGILState_Release(gstate);

            return result;
        });
    }

    return nullptr;
}

static Digiham::Mbe::MbeSynthesizer* createModule(std::string serverString) {
    if (!serverString.length()) {
        // no arguments given, use default behavior
        return new Digiham::Mbe::MbeSynthesizer();
    }
    if (serverString.at(0) == '/') {
        // is a unix domain socket path
        return new Digiham::Mbe::MbeSynthesizer(serverString);
    }

    // is an IPv4 / IPv6 address or hostname as string

    // default port
    unsigned short port = 1073;

    // split by port number, if given
    size_t pos = serverString.find(":");
    if (pos != std::string::npos) {
        port = std::stoul(serverString.substr(pos + 1));
        serverString = serverString.substr(0, pos);
    }

    return new Digiham::Mbe::MbeSynthesizer(serverString, port);
}

static int MbeSynthesizer_init(MbeSynthesizer* self, PyObject* args, PyObject* kwds) {
    self->inputFormat = FORMAT_CHAR;
    self->outputFormat = FORMAT_SHORT;

    static char* kwlist[] = {(char*) "mode", (char*) "server", NULL};

    PyTypeObject* ModeType = getAmbeModeType();
    char* server = (char*) "";
    PyObject* mode;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!|s", kwlist, ModeType, &mode, &server)) {
        Py_DECREF(ModeType);
        return -1;
    }
    Py_DECREF(ModeType);

    Digiham::Mbe::Mode* ambeMode = convertToAmbeMode(mode);
    if (ambeMode == nullptr) {
        PyErr_SetString(PyExc_ValueError, "unsupported ambe mode");
        return -1;
    }

    std::string serverString(server);

    // creating an mbesysnthesizer module potentially waits for network traffic, so we allow other threads in the meantime
    Digiham::Mbe::MbeSynthesizer* module = nullptr;
    std::string error;
    Py_BEGIN_ALLOW_THREADS
    try {
        module = createModule(serverString);
        module->setMode(ambeMode);
    } catch (const Digiham::Mbe::ConnectionError& e) {
        error = e.what();
    }
    Py_END_ALLOW_THREADS

    if (error != "") {
        PyErr_SetString(PyExc_ConnectionError, error.c_str());
        return -1;
    }

    if (module == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "unable to create MbeSynthesizer module");
        return -1;
    }

    self->setModule(module);

    return 0;
}

static PyObject* MbeSynthesizer_hasAmbe(MbeSynthesizer* self, PyObject* args, PyObject* kwds) {
    static char* kwlist[] = {(char*) "server", NULL};

    char* server = (char*) "";
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &server)) {
        return NULL;
    }

    std::string serverString(server);

    // creating an mbesysnthesizer module potentially waits for network traffic, so we allow other threads in the meantime
    bool result = false;
    std::string error;
    Py_BEGIN_ALLOW_THREADS
    try {
        Digiham::Mbe::MbeSynthesizer* module = createModule(serverString);
        result = module->hasAmbeCodec();
    } catch (const Digiham::Mbe::ConnectionError& e) {
        error = e.what();
    }
    Py_END_ALLOW_THREADS

    if (error != "") {
        PyErr_SetString(PyExc_ConnectionError, error.c_str());
        return NULL;
    }

    if (result) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyMethodDef MbeSynthesizer_methods[] = {
    {"hasAmbe", (PyCFunction) MbeSynthesizer_hasAmbe, METH_VARARGS | METH_KEYWORDS | METH_STATIC,
     "check if the codecserver instance supports the ambe codec"
    },
    {NULL}  /* Sentinel */
};

static PyType_Slot MbeSynthesizerSlots[] = {
    {Py_tp_init, (void*) MbeSynthesizer_init},
    {Py_tp_methods, MbeSynthesizer_methods},
    {0, 0}
};

PyType_Spec MbeSynthesizerSpec = {
    "digiham.modules.MbeSynthesizer",
    sizeof(MbeSynthesizer),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_FINALIZE,
    MbeSynthesizerSlots
};
