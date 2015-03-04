//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#include "pythonengine.hpp"
#ifdef HUGGLE_PYTHON
#include <cstdint>
#include "core.hpp"
#include "configuration.hpp"
#include "exception.hpp"
#include "localization.hpp"
#include "resources.hpp"
#include "syslog.hpp"
#include "query.hpp"
#include "wikiedit.hpp"
#include "wikisite.hpp"
#include "wikiuser.hpp"
#include "wikiutil.hpp"

using namespace Huggle;
using namespace Huggle::Python;

// let's define huggle api for python here
namespace Huggle
{
    namespace Python
    {
// Disable compiler warnings because we always need to use unused parameters when we use python
#if _MSC_VER
#pragma warning ( push )
#pragma warning ( disable )
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

////////////////////////////////////////////////////////////

        static void TryCatch(PyObject *script)
        {
            if (script)
            {
                PythonScript *ptr = Core::HuggleCore->Python->PythonScriptObjFromPyObj(script);
                if (ptr != nullptr)
                {
                    HUGGLE_DEBUG1("Exception in " + ptr->GetName());
                }
            }
            if (PyErr_Occurred())
            {
                HUGGLE_DEBUG("Printing the exception to cout::stderr", 5);
                PyErr_PrintEx(1);
            }
        }

        static PyObject *ApiVersion(PyObject *self, PyObject *args)
        {
            PyObject *v = PyUnicode_FromString(Configuration::HuggleConfiguration->HuggleVersion.toUtf8().data());
            return v;
        }

        QString StringFromPyString(PyObject *text)
        {
            if (!text)
                return "";
            PyObject *tn = PyUnicode_AsASCIIString(text);
            if (!tn)
            {
                TryCatch(nullptr);
                return "";
            }
            QString result = QString(PyBytes_AsString(tn));
            Py_DECREF(tn);
            return result;
        }

///////////////////////////////////////////////////////////////////////
// MARSHALLING
///////////////////////////////////////////////////////////////////////

// Functions here are turning our objects into python corresponding versions

// for most of the classes we just create a dict which has each variable mapped to a hash key

        static PyObject *QString2PyObject(QString text)
        {
            PyObject *rx = PyUnicode_FromString(text.toUtf8().data());
            if (!rx)
            {
                TryCatch(NULL);
                throw new Huggle::NullPointerException("rx", BOOST_CURRENT_FUNCTION);
            }

            return rx;
        }

        static PyObject *WikiSite2PyObject(WikiSite *Site)
        {
            PyObject *site = PyDict_New();
            if (site == nullptr)
                throw new Huggle::NullPointerException("site", BOOST_CURRENT_FUNCTION);

            PyObject *site_name_k = QString2PyObject("name");
            PyObject *site_name_v = QString2PyObject(Site->Name);
            if (PyDict_SetItem(site, site_name_k, site_name_v))
                goto error;
            Py_DECREF(site_name_k);
            Py_DECREF(site_name_v);
            return site;

            error:
                Py_DECREF(site_name_k);
                Py_DECREF(site_name_v);
                TryCatch(NULL);
                throw new Huggle::Exception("Can't turn WikiSite into PyObject", BOOST_CURRENT_FUNCTION);
        }

        static PyObject *WikiUser2PyObject(WikiUser *User)
        {
            PyObject *user = PyDict_New();
            if (user == nullptr)
                throw new Huggle::NullPointerException("user", BOOST_CURRENT_FUNCTION);

            PyObject *user_name_k = QString2PyObject("username");
            PyObject *user_name_v = QString2PyObject(User->Username);
            PyObject *user_site_k = QString2PyObject("site");
            PyObject *user_site_v = WikiSite2PyObject(User->GetSite());
            if (PyDict_SetItem(user_name_k, user, user_name_v))
                goto error;
            if (PyDict_SetItem(user_site_k, user, user_site_v))
                goto error;

            // we need to decref all values at same moment because in case there was an error and we throw
            // there is one extra decref in error block as we don't want to leak
            Py_DECREF(user_site_k);
            Py_DECREF(user_site_v);
            Py_DECREF(user_name_k);
            Py_DECREF(user_name_v);
            return user;

            error:
                Py_DECREF(user_site_k);
                Py_DECREF(user_site_v);
                Py_DECREF(user_name_k);
                Py_DECREF(user_name_v);
                TryCatch(NULL);
                throw new Huggle::Exception("Can't turn WikiUser into PyObject", BOOST_CURRENT_FUNCTION);
        }

        static PyObject *WikiPage2PyObject(WikiPage *Page)
        {
            PyObject *page = PyDict_New();

            return page;
        }

        static PyObject *WikiEdit2PyObject(WikiEdit *Edit)
        {
            PyObject *edit = PyDict_New();
            if (!edit)
                throw new Huggle::NullPointerException("edit", BOOST_CURRENT_FUNCTION);
            PyObject *edit_revid_k = QString2PyObject("revid");
            PyObject *edit_revid_v = PyLong_FromUnsignedLongLong(Edit->RevID);
            PyObject *edit_user_k = QString2PyObject("user");
            PyObject *edit_user_v = WikiUser2PyObject(Edit->User);
            PyObject *edit_page_k = QString2PyObject("page");
            PyObject *edit_page_v = WikiPage2PyObject(Edit->Page);
            if (PyDict_SetItem(edit, edit_revid_k, edit_revid_v))
                goto error;
            if (PyDict_SetItem(edit, edit_user_k, edit_user_v))
                goto error;
            if (PyDict_SetItem(edit, edit_page_k, edit_page_v))
                goto error;

            Py_DECREF(edit_revid_v);
            Py_DECREF(edit_revid_k);
            Py_DECREF(edit_user_k);
            Py_DECREF(edit_user_v);
            Py_DECREF(edit_page_k);
            Py_DECREF(edit_page_v);

            return edit;

            error:
                Py_DECREF(edit_revid_v);
                Py_DECREF(edit_revid_k);
                Py_DECREF(edit_user_k);
                Py_DECREF(edit_user_v);
                Py_DECREF(edit_page_k);
                Py_DECREF(edit_page_v);
                TryCatch(NULL);
                throw new Huggle::Exception("Unable to turn WikiEdit to PyObject", BOOST_CURRENT_FUNCTION);
        }

///////////////////////////////////////////////////////////////////////
// HUGGLE API'S
///////////////////////////////////////////////////////////////////////

        static PyObject *DebugLog(PyObject *self, PyObject *args)
        {
            PyObject *py_verbosity_ = nullptr;
            PyObject *text_ = nullptr;
            PyObject *result_ = PyBool_FromLong(0);
            if (PyArg_UnpackTuple(args, "debug_log", 1, 2, &text_, &py_verbosity_))
            {
                Py_DECREF(result_);
                result_ = PyBool_FromLong(1);
                unsigned int verbosity = 1;
                if (py_verbosity_ != nullptr && PyLong_Check(py_verbosity_))
                {
                    verbosity = (unsigned int)PyLong_AsLong(py_verbosity_);
                    Py_DECREF(py_verbosity_);
                }
                if (verbosity < 1)
                {
                    verbosity = 1;
                }
                PyObject *uni_ = PyUnicode_AsUTF8String(text_);
                Py_DECREF(text_);
                if (uni_ == nullptr || !PyBytes_Check(uni_))
                {
                    TryCatch(self);
                    Syslog::HuggleLogs->DebugLog("Log@unkown: parameter text must be of a string type");
                } else
                {
                    QString qs_text_(PyBytes_AsString(uni_));
                    Syslog::HuggleLogs->DebugLog(qs_text_, verbosity);
                    Py_DECREF(uni_);
                }
            } else
            {
                HUGGLE_DEBUG("Failed to unpack tuple @DebugLog", 2);
                TryCatch(self);
            }
            return result_;
        }

        static PyObject *Log_(HuggleLogType log_type, PyObject *self, PyObject *args)
        {
            PyObject *text_ = nullptr;
            PyObject *result_ = PyBool_FromLong(0);
            QString fc_name_;
            switch (log_type)
            {
                case HuggleLogType_Error:
                    fc_name_ = "error_log";
                    break;
                case HuggleLogType_Warn:
                    fc_name_ = "warn_log";
                    break;
                default:
                    fc_name_ = "log";
                    break;
            }
            if (PyArg_UnpackTuple(args, "log", 1, 1, &text_))
            {
                PyObject *uni_ = PyUnicode_AsUTF8String(text_);
                Py_DECREF(text_);
                if (uni_ == nullptr || !PyBytes_Check(uni_))
                {
                    Syslog::HuggleLogs->DebugLog("Python::$" + fc_name_ + "@unkown: parameter text must be of a string type");
                } else
                {
                    Py_DECREF(result_);
                    result_ = PyBool_FromLong(1);
                    QString qs_text_(PyBytes_AsString(uni_));
                    Py_DECREF(uni_);
                    switch (log_type)
                    {
                        case HuggleLogType_Error:
                            Syslog::HuggleLogs->ErrorLog(qs_text_);
                            break;
                        case HuggleLogType_Warn:
                            Syslog::HuggleLogs->WarningLog(qs_text_);
                            break;
                        default:
                            Syslog::HuggleLogs->Log(qs_text_);
                            break;
                    }
                }
            } else
            {
                HUGGLE_DEBUG1("Failed to unpack tuple in Log_ fc");
                TryCatch(nullptr);
            }
            return result_;
        }

        static bool ContainsFunction(QString function, QString string)
        {
            QStringList lines = string.split('\n');
            QRegExp *regex_ = new QRegExp("^[\\s\\t]*def " + function + ".*:[\\s\\t]*$");
            int ln = 0;
            while (ln < lines.count())
            {
                if (lines.at(ln).contains(*regex_))
                {
                    delete regex_;
                    return true;
                }
                ln++;
            }

            delete regex_;
            return false;
        }

        static PyObject *Configuration_GetUser(PyObject *self, PyObject *args)
        {
            PyObject *result_ = PyUnicode_FromString(Configuration::HuggleConfiguration->SystemConfig_Username.toUtf8().data());
            return result_;
        }

        static PyObject *Configuration_GetWikiURL(PyObject *self, PyObject *args)
        {
            PyObject *result_ = PyUnicode_FromString(Configuration::HuggleConfiguration->GetProjectWikiURL().toUtf8().data());
            return result_;
        }

        static PyObject *Configuration_GetScript(PyObject *self, PyObject *args)
        {
            PyObject *result_ = PyUnicode_FromString(Configuration::HuggleConfiguration->GetProjectScriptURL().toUtf8().data());
            return result_;
        }

        static PyObject *Localize(PyObject *self, PyObject *args)
        {
            PyObject *string;
            if (!PyArg_UnpackTuple(args, "localize", 1, 1, &string))
            {
                TryCatch(self);
                return nullptr;
            }
            if (!string)
            {
                TryCatch(self);
                return nullptr;
            }
            PyObject *result_ = PyUnicode_FromString(_l(StringFromPyString(string)).toUtf8().data());
            // remove the temp objects we created
            Py_DECREF(string);

            // localized text
            return result_;
        }

        static PyObject *Config_Set(PyObject *self, PyObject *args)
        {
            PyObject *name, *vals, *extension;
            if (!PyArg_UnpackTuple(args, "Config_Set", 1, 3, &extension, &name, &vals))
            {
                TryCatch(self);
                return nullptr;
            }
            QString option = StringFromPyString(name);
            QString ex = StringFromPyString(extension);
            QString data = StringFromPyString(vals);
            // remove the temp objects we created
            Py_DECREF(name);
            Py_DECREF(vals);
            if (!Configuration::HuggleConfiguration->ExtensionData.contains(ex))
            {
                // it seems this extension didn't store any data so far
                // let's create a new storage
                Configuration::HuggleConfiguration->ExtensionData.insert(ex, new ExtensionConfig());
            }
            Configuration::HuggleConfiguration->ExtensionData[ex]->SetOption(option, data);
            PyObject *result_ = PyBool_FromLong(1);
            return result_;
        }

        static PyObject *Config_Get(PyObject *self, PyObject *args)
        {
            PyObject *name, *extension;
            if (!PyArg_UnpackTuple(args, "Config_Set", 1, 2, &extension, &name))
            {
                TryCatch(self);
                return nullptr;
            }
            QString option = StringFromPyString(name);
            QString module = StringFromPyString(extension);
            // remove the temp objects we created
            Py_DECREF(name);
            Py_DECREF(extension);
            PyObject *result_ = PyUnicode_FromString(Configuration::HuggleConfiguration->GetExtensionConfig(module, option, "").toUtf8().data());
            return result_;
        }

        static PyObject *Log(PyObject *self, PyObject *args)
        {
            return Log_(HuggleLogType_Normal, self, args);
        }

        static PyObject *WarningLog(PyObject *self, PyObject *args)
        {
            return Log_(HuggleLogType_Warn, self, args);
        }

        static PyObject *ErrorLog(PyObject *self, PyObject *args)
        {
            return Log_(HuggleLogType_Error, self, args);
        }

        static PyObject *Wikipage_append2(PyObject *self, PyObject *args, bool reference)
        {
            HUGGLE_DEBUG("Running Wikipage_append", 4);
            PyObject *result;
            PyObject *page, *text, *summary;
            if (!PyArg_UnpackTuple(args, "Wikipage_Append", 1, 3, &page, &text, &summary))
            {
                HUGGLE_DEBUG1("Failed to run Wikipage_append - arguments could not be processed");
                return nullptr;
            }
            PyObject *tpage = PyUnicode_AsASCIIString(page);
            PyObject *tsuma = PyUnicode_AsASCIIString(summary);
            PyObject *ttext = PyUnicode_AsASCIIString(text);
            if (!tpage || !tsuma || !ttext)
            {
                TryCatch(self);
                return nullptr;
            }
            QString page_name = QString(PyBytes_AsString(tpage));
            QString page_text = QString(PyBytes_AsString(ttext));
            QString page_su = QString(PyBytes_AsString(tsuma));
            // remove the temp objects we created
            Py_DECREF(ttext);
            Py_DECREF(tsuma);
            Py_DECREF(tpage);
            // now we need to append the text to page
            Collectable_SmartPtr<EditQuery> ptr = WikiUtil::AppendTextToPage(page_name, page_text, page_su);
            Py_DECREF(page);
            Py_DECREF(summary);
            Py_DECREF(text);
            if (reference)
            {
                result = PyLong_FromUnsignedLong(Core::HuggleCore->Python->InsertQuery(ptr));
                return result;
            } else
            {
                result = PyBool_FromLong(1);
                return result;
            }
        }

        static PyObject *Wikipage_rappend(PyObject *self, PyObject *args)
        {
            return Wikipage_append2(self, args, true);
        }

        static PyObject *Wikipage_append(PyObject *self, PyObject *args)
        {
            return Wikipage_append2(self, args, false);
        }

        static PyObject *DeleteQuery(PyObject *self, PyObject *args)
        {
            PyObject *query;
            PyObject *result;
            if (!PyArg_UnpackTuple(args, "DeleteQuery", 1, 1, &query))
            {
                TryCatch(self);
                return nullptr;
            }
            unsigned long id = PyLong_AsUnsignedLong(query);
            Py_DecRef(query);
            result = PyLong_FromUnsignedLong(Core::HuggleCore->Python->RemoveQuery(id));
            return result;
        }

        static PyObject *Query_IsProcessed(PyObject *self, PyObject *args)
        {
            PyObject *query;
            PyObject *result;
            if (!PyArg_UnpackTuple(args, "DeleteQuery", 1, 1, &query))
            {
                TryCatch(self);
                return nullptr;
            }
            unsigned long id = PyLong_AsUnsignedLong(query);
            Py_DecRef(query);
            Query *qr = Core::HuggleCore->Python->GetQuery(id);
            if (!qr)
            {
                // there is no such a query
                result = PyErr_NewException("EINVALIDQUERY", self, nullptr);
                PyErr_SetString(result, "There is no such a query with this reference (did you delete it?)");
            } else
            {
                if (qr->IsProcessed())
                    result = PyBool_FromLong(1);
                else
                    result = PyBool_FromLong(0);
            }
            return result;
        }

        static PyObject *Query_IsFailed(PyObject *self, PyObject *args)
        {
            PyObject *query;
            PyObject *result;
            if (!PyArg_UnpackTuple(args, "DeleteQuery", 1, 1, &query))
            {
                TryCatch(self);
                return nullptr;
            }
            unsigned long id = PyLong_AsUnsignedLong(query);
            Py_DecRef(query);
            Query *qr = Core::HuggleCore->Python->GetQuery(id);
            if (!qr)
            {
                // there is no such a query
                result = PyErr_NewException("EINVALIDQUERY", self, nullptr);
                PyErr_SetString(result, "There is no such a query with this reference (did you delete it?)");
            } else
            {
                if (qr->IsFailed())
                    result = PyBool_FromLong(1);
                else
                    result = PyBool_FromLong(0);
            }
            return result;
        }

        static PyObject *Wikipage_edit(PyObject *self, PyObject *args)
        {
            HUGGLE_DEBUG("Running Wikipage_edit", 4);
            PyObject *result;
            PyObject *page, *text, *summary;
            if (!PyArg_UnpackTuple(args, "Wikipage_edit", 1, 3, &page, &text, &summary))
            {
                TryCatch(self);
                HUGGLE_DEBUG1("Failed to run Wikipage_edit - arguments could not be processed");
                return nullptr;
            }
            PyObject *tpage = PyUnicode_AsASCIIString(page);
            PyObject *tsuma = PyUnicode_AsASCIIString(summary);
            PyObject *ttext = PyUnicode_AsASCIIString(text);
            if (!tpage || !tsuma || !ttext)
            {
                TryCatch(self);
                return nullptr;
            }
            QString page_name = QString(PyBytes_AsString(tpage));
            QString page_text = QString(PyBytes_AsString(ttext));
            QString page_su = QString(PyBytes_AsString(tsuma));
            // remove the temp objects we created
            Py_DECREF(ttext);
            Py_DECREF(tsuma);
            Py_DECREF(tpage);
            // now we need to append the text to page
            WikiUtil::EditPage(page_name, page_text, page_su);
            Py_DECREF(page);
            Py_DECREF(summary);
            Py_DECREF(text);

            result = PyBool_FromLong(1);
            return result;
        }

#if _MSC_VER
#pragma warning ( pop )
#else
#pragma GCC diagnostic pop
#endif

        static PyMethodDef Methods[] = {
            {"configuration_get_user", Configuration_GetUser, METH_VARARGS, "Request a name of user who is currently used on selected wiki"},
            {"configuration_get_project_script_url", Configuration_GetScript, METH_VARARGS, "Return a script url"},
            {"configuration_get_project_wiki_url", Configuration_GetWikiURL, METH_VARARGS, "Return an URL of current project"},
            {"configuration_get", Config_Get, METH_VARARGS, "Get a private configuration option"},
            {"configuration_set", Config_Set, METH_VARARGS, "Set a private configuration option"},
            {"delete_query", DeleteQuery, METH_VARARGS, ""},
            {"localize", Localize, METH_VARARGS, ""},
            {"huggle_version", ApiVersion, METH_VARARGS, "Return a huggle version"},
            {"warning_log", WarningLog, METH_VARARGS, "Write to warning log"},
            {"log", Log, METH_VARARGS, "Write to a log"},
            {"debug_log", DebugLog, METH_VARARGS, "Write to debug log"},
            {"error_log", ErrorLog, METH_VARARGS, "Write to error log using stderr to output"},
            {"query_isfailed", Query_IsFailed, METH_VARARGS, "Return Query::IsFailed as a py_bool"},
            {"query_isprocessed", Query_IsProcessed, METH_VARARGS, "Return Query::IsProcessed as a bool"},
            {"wikipage_append", Wikipage_append, METH_VARARGS, "Append a text to a page"},
            {"wikipage_rappend", Wikipage_rappend, METH_VARARGS, "Append a text to a page"},
            {"wikipage_edit", Wikipage_edit, METH_VARARGS, "Edit a page"},
            {nullptr, nullptr, 0, nullptr}
        };


        static PyObject *Huggle_ptr;

        #if PY_MAJOR_VERSION >= 3
            static PyModuleDef Module = {
                PyModuleDef_HEAD_INIT, "huggle", nullptr, -1, Methods, nullptr, nullptr, nullptr, nullptr
            };

            static PyObject *PyInit_emb()
        #else
            static void PyInit_emb()
        #endif
            {
                Huggle_ptr = PyModule_Create2(&Module, PYTHON_API_VERSION);
                PyModule_AddIntConstant(Huggle_ptr, "SUCCESS", HUGGLE_SUCCESS);
                PyModule_AddIntConstant(Huggle_ptr, "EINVALIDQUERY", HUGGLE_EINVALIDQUERY);
                PyModule_AddIntConstant(Huggle_ptr, "ENOTLOGGEDIN", HUGGLE_ENOTLOGGEDIN);
                PyModule_AddIntConstant(Huggle_ptr, "EUNKNOWN", HUGGLE_EUNKNOWN);
                PyModule_AddStringConstant(Huggle_ptr, "HUGGLE_VERSION", HUGGLE_VERSION);
        #if PY_MAJOR_VERSION >= 3
                return Huggle_ptr;
        #endif
            }
    }
}

static QString DoubleBack(QString path)
{
    QString result = QString(path);
    return result.replace("\\", "\\\\");
}

PythonEngine::PythonEngine(QString ExtensionsFolder_)
{
    this->hugglePyLib = Resources::GetResource("/huggle/text/Resources/Python/definitions.py");
    // define hooks
    PyImport_AppendInittab("huggle", &PyInit_emb);
    // load it
    Py_Initialize();
    Syslog::HuggleLogs->DebugLog("Inserting extensions folder to path: " + DoubleBack(ExtensionsFolder_));
    if (PyRun_SimpleString(QString("import sys; sys.path.append('" + DoubleBack(ExtensionsFolder_) + "')").toUtf8().data()))
        TryCatch(nullptr);
#ifdef HUGGLE_GLOBAL_EXTENSION_PATH
    if (PyRun_SimpleString(QString(QString("sys.path.append('") + HUGGLE_GLOBAL_EXTENSION_PATH + "')").toUtf8().data()))
    {
        TryCatch(nullptr);
    }
#endif
}

PythonEngine::~PythonEngine()
{
    while (this->Scripts.count() > 0)
    {
        delete this->Scripts.at(0);
        this->Scripts.removeAt(0);
    }
    QList<unsigned long> ks = this->Queries.keys();
    foreach (unsigned long id, ks)
    {
        // fix all leaks caused by python
        this->Queries[id]->UnregisterConsumer(HUGGLECONSUMER_PYTHON);
    }
    Py_Finalize();
}

bool PythonEngine::LoadScript(QString path)
{
    PythonScript *p = new PythonScript(path);
    this->Scripts.append(p);
    if (p->Init())
    {
        p->SetEnabled(true);
        return true;
    }
    return false;
}

QList<PythonScript *> PythonEngine::ScriptsList()
{
    return QList<PythonScript*>(this->Scripts);
}

Query *PythonEngine::GetQuery(unsigned long ID)
{
    if (this->Queries.contains(ID))
        return this->Queries[ID];

    // when there is no such a query we return a null pointer
    // that means there is no such a query
    return nullptr;
}

unsigned long PythonEngine::InsertQuery(Query *query)
{
    unsigned long ID = this->LastQuery;
    query->RegisterConsumer(HUGGLECONSUMER_PYTHON);
    this->LastQuery++;
    this->Queries.insert(ID, query);
    return ID;
}

unsigned long PythonEngine::RemoveQuery(unsigned long ID)
{
    if (this->Queries.contains(ID))
    {
        // let's remove it
        this->Queries[ID]->UnregisterConsumer(HUGGLECONSUMER_PYTHON);
        this->Queries.remove(ID);
        return HUGGLE_SUCCESS;
    }
    return HUGGLE_EINVALIDQUERY;
}

PythonScript *PythonEngine::PythonScriptObjFromPyObj(PyObject *object)
{
    foreach (PythonScript *script, this->Scripts)
    {
        if (object == script->PythonObject())
        {
            return script;
        }
    }
    HUGGLE_DEBUG("Unable to resolve script from script table, id: 0x" + QString::number((intptr_t)object, 16), 4);
    return nullptr;
}

unsigned int PythonEngine::Count()
{
    unsigned int s = 0;
    foreach (PythonScript *x, this->Scripts)
    {
        // we only want to know how many enabled scripts we have
        if (x->IsEnabled())
        {
            s++;
        }
    }
    return s;
}

void PythonEngine::Hook_HuggleShutdown()
{
    foreach (PythonScript *c, this->Scripts)
    {
        if (c->IsEnabled())
        {
            c->Hook_Shutdown();
        }
    }
}

void PythonEngine::Hook_MainWindowIsLoaded()
{
    foreach (PythonScript *c, this->Scripts)
    {
        if (c->IsEnabled())
        {
            c->Hook_MainWindowIsLoaded();
        }
    }
}

void PythonEngine::Hook_SpeedyFinished(WikiEdit *edit, QString tags, bool successfull)
{
    foreach (PythonScript *c, this->Scripts)
    {
        if (c->IsEnabled())
        {
            c->Hook_SpeedyFinished(edit, tags, successfull);
        }
    }
}

PythonScript::PythonScript(QString name)
{
    this->SourceCode = "";
    this->Name = name;
    this->object = nullptr;
    this->ModuleID = name;
    if (name.endsWith(".py"))
    {
        this->ModuleID = this->ModuleID.mid(0, this->ModuleID.length() - 3);
    }
    if (this->ModuleID.contains("/"))
    {
        this->ModuleID = this->ModuleID.mid(this->ModuleID.lastIndexOf("/") + 1);
    }
    if (this->ModuleID.contains("\\"))
    {
        this->ModuleID = this->ModuleID.mid(this->ModuleID.lastIndexOf("\\") + 1);
    }
    this->ptr_Hook_MainLoaded = nullptr;
    this->Description = "<unknown>";
    this->Author = "<unknown>";
    this->Version = "<unknown>";
    this->Enabled = false;
}

PythonScript::~PythonScript()
{
    if (this->ptr_Hook_MainLoaded != nullptr)
    {
        Py_DECREF(this->ptr_Hook_MainLoaded);
    }
    if (this->object != nullptr)
    {
        Py_DECREF(this->object);
    }
}

PyObject *PythonScript::Hook(QString function)
{
    PyObject *ptr_python_ = nullptr;
    if (Huggle::Python::ContainsFunction(function, this->SourceCode))
    {
        Syslog::HuggleLogs->DebugLog("Loading hook symbols of " + function + " " + this->Name, 2);
        ptr_python_ = PyObject_GetAttrString(this->object, function.toUtf8().data());
        if (ptr_python_ != nullptr && !PyCallable_Check(ptr_python_))
        {
            if (Configuration::HuggleConfiguration->Verbosity > 0)
                TryCatch(nullptr);
            // we loaded the symbol but it's not callable function
            // so we remove it
            Py_DECREF(ptr_python_);
            Syslog::HuggleLogs->WarningLog("Function " + function + "@" + this->Name
                                           + " isn't callable, hook is disabled now");
            ptr_python_ = nullptr;
        } else if (ptr_python_ == nullptr)
        {
            Syslog::HuggleLogs->DebugLog("There is no override for " + function);
            if (Configuration::HuggleConfiguration->Verbosity > 0)
                TryCatch(nullptr);
        }
    }
    return ptr_python_;
}

QString PythonScript::CallInternal(QString function)
{
    PyObject *ptr_python_ = nullptr;
    if (Huggle::Python::ContainsFunction(function, this->SourceCode))
    {
        Syslog::HuggleLogs->DebugLog("Loading symbols of " + function + " " + this->Name, 2);
        ptr_python_ = PyObject_GetAttrString(this->object, function.toUtf8().data());
        if (ptr_python_ != nullptr && !PyCallable_Check(ptr_python_))
        {
            // we loaded the symbol but it's not callable function
            // so we remove it
            Py_DECREF(ptr_python_);
            Syslog::HuggleLogs->WarningLog("Function " + function + "@" + this->Name
                                           + " isn't callable, unable to retrieve");
            TryCatch(nullptr);
            ptr_python_ = nullptr;
        } else if (ptr_python_ == nullptr)
        {
            Syslog::HuggleLogs->DebugLog("There is no override for " + function);
            TryCatch(nullptr);
        }
    }
    if (ptr_python_ == nullptr)
    {
        return "<unknown>";
    }
    PyObject *value = PyObject_CallObject(ptr_python_, nullptr);
    PyObject *text_ = PyUnicode_AsUTF8String(value);
    Py_DECREF(value);
    if (text_ == nullptr || !PyBytes_Check(text_))
    {
        TryCatch(nullptr);
        Syslog::HuggleLogs->DebugLog("Python::$" + function + "@" + this->Name + ": return value must be of a string type");
        return "<unknown>";
    } else
    {
        QString qs_text_(PyBytes_AsString(text_));
        Py_DECREF(text_);
        return qs_text_;
    }
}

QString PythonScript::GetName() const
{
    return this->Name;
}

QString PythonScript::GetModule() const
{
    return this->ModuleID;
}

bool PythonScript::IsEnabled() const
{
    return this->Enabled;
}

void PythonScript::SetEnabled(bool value)
{
    this->Enabled = value;
}

/////////////////////////////////////////////////
// HOOKS
/////////////////////////////////////////////////

void PythonScript::Hook_Shutdown()
{
    if (this->ptr_Hook_Shutdown != nullptr)
    {
        HUGGLE_DEBUG("Calling hook Hook_Shutdown @" + this->Name, 2);
        if (!PyObject_CallObject(this->ptr_Hook_Shutdown, nullptr))
            TryCatch(nullptr);
    }
}

void PythonScript::Hook_SpeedyFinished(WikiEdit *edit, QString tags, bool successfull)
{
    if (edit == nullptr)
        return;
    if (this->ptr_Hook_SpeedyFinished != nullptr)
    {
        HUGGLE_DEBUG("Calling hook Hook_SpeedyFinished @" + this->Name, 2);
        // let's make a new list of params
        PyObject *page_name = PyUnicode_FromString(edit->Page->PageName.toUtf8().data());
        if (!page_name)
            goto error;
        PyObject *page_t_ = PyUnicode_FromString(tags.toUtf8().data());
        if (!page_t_)
            goto error;
        PyObject *user_name = PyUnicode_FromString(edit->User->Username.toUtf8().data());
        if (!user_name)
            goto error;
        PyObject *success;
        if (!successfull)
            success = PyUnicode_FromString("fail");
        else
            success = PyUnicode_FromString("success");
        if (!success)
            goto error;
        PyObject *args = PyTuple_Pack(4, page_name, user_name, page_t_, success);
        if (!args)
            goto error;
        if (!PyObject_CallObject(this->ptr_Hook_SpeedyFinished, args))
            goto error;
    }
    return;

    error:
        HUGGLE_DEBUG("Error in: " + this->Name, 2);
        TryCatch(nullptr);

}

void PythonScript::Hook_MainWindowIsLoaded()
{
    if (this->ptr_Hook_MainLoaded != nullptr)
    {
        Syslog::HuggleLogs->DebugLog("Calling hook Hook_MainWindowIsLoaded @" + this->Name, 2);
        if(!PyObject_CallObject(this->ptr_Hook_MainLoaded, nullptr))
            TryCatch(nullptr);
    }
}

bool PythonScript::Init()
{
    QFile *file = new QFile(this->Name);
    if (QFile().exists(this->Name))
    {
        if (!file->open(QIODevice::ReadOnly))
        {
            Huggle::Syslog::HuggleLogs->ErrorLog("Unable to open " + this->Name);
            delete file;
            return false;
        }
        this->SourceCode = QString(file->readAll());
        file->close();
        delete file;
        HUGGLE_DEBUG("Importing module " + this->ModuleID, 4);
        PyObject *name = PyUnicode_FromString(this->ModuleID.toUtf8().data());
        if (name == nullptr)
        {
            TryCatch(nullptr);
            return false;
        }
        this->object = PyImport_Import(name);
        // remove name
        Py_DECREF(name);
        if (this->object == nullptr)
        {
            HUGGLE_DEBUG("Failed to load module " + this->ModuleID, 2);
            TryCatch(nullptr);
            return false;
        }
        Syslog::HuggleLogs->DebugLog("Loading hook symbols for python " + this->Name);
        // load symbols for hooks now
        this->ptr_Hook_Shutdown = this->Hook("hook_shutdown");
        this->ptr_Hook_SpeedyFinished = this->Hook("hook_speedy_finished");
        this->ptr_Hook_MainLoaded = this->Hook("hook_main_window_is_loaded");

        // load the information about the plugin
        this->Author = this->CallInternal("get_author");
        this->Description = this->CallInternal("get_description");
        this->Version = this->CallInternal("get_version");
        return true;
    }
    delete file;
    return false;
}

QString PythonScript::RetrieveSourceCode() const
{
    return this->SourceCode;
}

QString PythonScript::GetVersion() const
{
    return this->Version;
}

QString PythonScript::GetAuthor()
{
    return this->Author;
}

QString PythonScript::GetDescription() const
{
    return this->Description;
}

#endif
