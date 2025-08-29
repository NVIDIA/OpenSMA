/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <python3.12/Python.h>

extern "C" {
#include <gnumake.h>
}

// required for gnumake
int plugin_is_GPL_compatible = 1;

// the two modules we are exporting to python
constexpr auto ModuleName    = "ubs";
constexpr auto ModuleNameGmk = "ubs_gmk";

namespace {

// per module dicts
struct Dicts
{
    PyObject* globals = nullptr;
    // PyObject* locals  = nullptr;
};
std::unordered_map<std::string, Dicts> _dicts;

// globals to hold global module and dict
PyObject* ubs_obj     = nullptr;
PyObject* global_dict = nullptr;

// called at shutdown
void ubs_gmk_cleanup()
{
    PyGILState_Ensure();  // make it safe to run Python
    Py_Finalize();
}

// called on exit, with exit value.
void ubs_gmk_exit(int val, void* args)
{
    // dump out PASSED or FAILED banners
    auto path = gmk_expand("$(UBS_PATH_THEME)");
    auto want = gmk_expand("$(filter $(MAKECMDGOALS),$(UBS_COMMANDS_RESULT))");
    if (std::string(want).empty() == false) {
        std::stringstream ss;
        ss << path << '/' << (val ? "failed" : "passed");
        std::cout << std::ifstream(ss.str().c_str()).rdbuf();
    }
}

/// return true if error occurred.
bool ubs_python_error_check(const char* estr = nullptr)
{
    if (PyErr_Occurred()) {
        PyObject* type{};
        PyObject* value{};
        PyObject* traceback{};
        PyErr_Fetch(&type, &value, &traceback);
        auto pystr = PyObject_Str(value);
        auto str   = PyUnicode_AsUTF8(pystr);
        Py_DECREF(type);
        Py_DECREF(value);

        auto pre = gmk_expand("$(ubs-error-fmt)");
        std::cerr << pre << " ubs-py: " << str << std::endl;
        if (estr) {
            std::cerr << pre << "       : " << estr << std::endl;
        }
        Py_DECREF(str);
        Py_DECREF(pystr);

        // get extended debug info
        if (traceback != nullptr) {
            auto mod  = PyImport_ImportModule("traceback");
            auto dict = PyModule_GetDict(mod);
            auto tb   = PyDict_GetItemString(dict, "format_tb");
            auto tbl  = PyObject_CallFunctionObjArgs(tb, traceback, nullptr);
            Py_DECREF(traceback);

            if (tbl) {
                auto tstr = PyUnicode_Join(PyUnicode_FromString(""), tbl);
                auto cstr = PyUnicode_AsUTF8(tstr);
                Py_DECREF(tstr);
                Py_DECREF(tbl);
                if (cstr) {
                    std::cerr << cstr << std::endl;
                }
            }
            Py_DECREF(tb);
            Py_DECREF(dict);
            Py_DECREF(mod);
        }

        return true;
    }
    return false;
}

/// our python evalulator macro $(ubs-py)
char* ubs_py_base(const char* nm, uint32_t argc, char** argv, int begin)
{
    char* ret = nullptr;

    if (argc == 1) {
        PyObject* globals = global_dict;
        PyObject* locals  = PyDict_New();
        auto      dicts   = _dicts.find(nm);
        if (dicts != _dicts.end()) {
            globals = dicts->second.globals ? dicts->second.globals : global_dict;
        }

        auto res = PyRun_String(argv[0], begin, globals, locals);
        if (ubs_python_error_check(argv[0])) {
            std::exit(-1);  // this is fatal
            return nullptr;
        }
        Py_DECREF(locals);

        auto fun = PyObject_GetAttrString(ubs_obj, "_ubs_python_to_make");  // ref
        if (fun && PyCallable_Check(fun)) {
            auto arg = PyTuple_New(1);
            PyTuple_SetItem(arg, 0, res);
            auto val = PyObject_CallObject(fun, arg);
            Py_DECREF(fun);
            Py_DECREF(arg);

            if (val && PyUnicode_Check(val)) {
                auto str = PyUnicode_AsUTF8(val);
                ret      = gmk_alloc((str ? strlen(str) : 0) + 1);
                strcpy(ret, str);
                Py_DECREF(val);
            }
        }

        if (ubs_python_error_check()) {
            std::exit(-1);  // this is fatal
            return nullptr;
        }
    }
    else {
        auto fatal = gmk_expand("$(ubs-fatal-fmt)");
        std::cerr << fatal << " invalid call " << nm << std::endl;
    }
    return ret;
}

/// single line python evalulator macro $(ubs-py)
char* ubs_py(const char* nm, uint32_t argc, char** argv)
{
    return ubs_py_base(nm, argc, argv, Py_eval_input);
}
/// file python evalulator macro $(ubs-py)
char* ubs_py_exec(const char* nm, uint32_t argc, char** argv)
{
    return ubs_py_base(nm, argc, argv, Py_file_input);
}

/// make expansion of input string
PyObject* ubs_gmk_expand(PyObject* self, PyObject* args)
{
    const char* str{};
    if (!PyArg_ParseTuple(args, "s", &str)) {
        return nullptr;
    }

    auto mstr = gmk_expand(str);
    if (mstr) {
        auto pstr = PyUnicode_FromString(mstr);
        gmk_free(mstr);
        return pstr;
    }
    ubs_python_error_check("gmk_expand failed");
    return nullptr;
}

/// make evaluation of input string
PyObject* ubs_gmk_eval(PyObject* self, PyObject* args)
{
    const char* str{};
    if (!PyArg_ParseTuple(args, "s", &str)) {
        return nullptr;
    }

    gmk_floc floc{__FILE__, __LINE__};  // TODO: wrong
    gmk_eval(str, &floc);
    return Py_None;
}

/// used to execute any registered python function
char* ubs_function(const char* id, uint32_t argc, char** argv)
{
    std::stringstream ss;
    ss << id << "(";
    for (uint32_t i = 0; i < argc; i++) {
        ss << argv[i] << (i + 1 == argc ? "" : ",");
    }
    ss << ")";
    auto  args = ss.str();
    char* argv2[]{const_cast<char*>(args.c_str())};

    return ubs_py_base(id, 1, argv2, Py_eval_input);
}

/// used to expose a python function as a first class make function
PyObject* ubs_gmk_register(PyObject* self, PyObject* args)
{
    PyObject* obj{};
    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return nullptr;
    }

    auto id = PyEval_GetFuncName(obj);

    if (!PyCallable_Check(obj)) {
        auto pre = gmk_expand("$(ubs-fatal-fmt)");
        std::cerr << pre << "ubs-py: " << id << " is not callable" << std::endl;
        std::exit(2);
    }

    auto code_object = PyObject_GetAttrString(obj, "__code__");
    auto co_argcount = PyObject_GetAttrString(code_object, "co_argcount");
    long num_args    = PyLong_AsLong(co_argcount);
    Py_DECREF(code_object);
    Py_DECREF(co_argcount);

    // get references to the module dicts
    Dicts d{};
    d.globals = PyEval_GetGlobals();
    _dicts.emplace(id, d);

    // add the new function as a first class make function
    gmk_add_function(id, ubs_function, 0, num_args, 0);
    return Py_None;
}

// we only export three methods, register deals with the rest
PyMethodDef UbsMethods[] = {
    {  "expand",   ubs_gmk_expand, METH_VARARGS,              "expands as make."},
    {    "eval",     ubs_gmk_eval, METH_VARARGS,                "evals as make."},
    {"register", ubs_gmk_register, METH_VARARGS, "registers a new make function"},
    {      NULL,             NULL,            0,                            NULL}
};

PyModuleDef UbsModule = {
    PyModuleDef_HEAD_INIT, ModuleNameGmk, NULL, -1, UbsMethods, NULL, NULL, NULL, NULL};

PyObject* ubs_pyinit()
{
    return PyModule_Create(&UbsModule);
}

}  // namespace

// called at load lib/ubs.so time
extern "C" int ubs_gmk_setup()
{
    // setup the PYTHONPATH
    auto pypath = getenv("PYTHONPATH");

    std::stringstream ss;
    if (pypath) {
        ss << pypath << ":";
    }
    ss << gmk_expand("$(UBS_PATH_ROOT)");
    setenv("PYTHONPATH", ss.str().c_str(), 1);

    // move __pycache__ to build dir
    setenv("PYTHONPYCACHEPREFIX", gmk_expand("$(UBS_PATH_BUILD)/pycache"), 1);

    // Initialise an interpreter
    PyImport_AppendInittab(ModuleNameGmk, ubs_pyinit);
    Py_Initialize();

    // add shutdown hook
    atexit(ubs_gmk_cleanup);
    on_exit(ubs_gmk_exit, nullptr);

    // add function to evaluate python code
    gmk_add_function("ubs_py", ubs_py, 1, 1, 0);
    gmk_add_function("ubs_py_exec", ubs_py_exec, 1, 1, 0);

    // load up the ubs startup module into our interpreters global namespace
    auto init = PyUnicode_DecodeFSDefault(ModuleName);
    ubs_obj   = PyImport_Import(init);
    Py_DECREF(init);

    if (ubs_python_error_check()) {
        return 0;  // let make know it failed
    }

    // get the global dict so that we can use it
    global_dict = PyModule_GetDict(ubs_obj);

    return 1;  // success
}
