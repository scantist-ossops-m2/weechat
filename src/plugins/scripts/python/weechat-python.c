/*
 * Copyright (c) 2003-2008 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* weechat-python.c: Python plugin for WeeChat */

#undef _

#include <Python.h>

#include "../../weechat-plugin.h"
#include "../script.h"
#include "weechat-python.h"
#include "weechat-python-api.h"


WEECHAT_PLUGIN_NAME("python");
WEECHAT_PLUGIN_DESCRIPTION("Python plugin for WeeChat");
WEECHAT_PLUGIN_AUTHOR("FlashCode <flashcode@flashtux.org>");
WEECHAT_PLUGIN_VERSION(WEECHAT_VERSION);
WEECHAT_PLUGIN_WEECHAT_VERSION(WEECHAT_VERSION);
WEECHAT_PLUGIN_LICENSE("GPL3");

struct t_weechat_plugin *weechat_python_plugin = NULL;

struct t_plugin_script *python_scripts = NULL;
struct t_plugin_script *python_current_script = NULL;
char *python_current_script_filename = NULL;
PyThreadState *python_mainThreadState = NULL;

char python_buffer_output[128];


/*
 * weechat_python_exec: execute a Python script
 */

void *
weechat_python_exec (struct t_plugin_script *script,
		     int ret_type, char *function, char **argv)
{
    PyObject *evMain;
    PyObject *evDict;
    PyObject *evFunc;
    PyObject *rc;
    void *ret_value;
    int *ret_i;
    struct t_plugin_script *old_python_current_script;
    
    /* PyEval_AcquireLock (); */
    PyThreadState_Swap (script->interpreter);
    
    evMain = PyImport_AddModule ((char *) "__main__");
    evDict = PyModule_GetDict (evMain);
    evFunc = PyDict_GetItemString (evDict, function);
    
    if ( !(evFunc && PyCallable_Check (evFunc)) )
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s unable to run function \"%s\""),
                        weechat_prefix ("error"), "python", function);
	/* PyEval_ReleaseThread (python_current_script->interpreter); */
	return NULL;
    }

    old_python_current_script = python_current_script;
    python_current_script = script;
    
    if (argv && argv[0])
    {
        if (argv[1])
        {
            if (argv[2])
            {
                if (argv[3])
                {
                    if (argv[4])
                    {
                        if (argv[5])
                        {
                            rc = PyObject_CallFunction (evFunc, "ssssss", argv[0],
                                                        argv[1], argv[2], argv[3],
                                                        argv[4], argv[5]);
                        }
                        else
                            rc = PyObject_CallFunction (evFunc, "sssss", argv[0],
                                                        argv[1], argv[2], argv[3],
                                                        argv[4]);
                    }
                    else
                        rc = PyObject_CallFunction (evFunc, "ssss", argv[0],
                                                    argv[1], argv[2], argv[3]);
                }
                else
                    rc = PyObject_CallFunction (evFunc, "sss", argv[0],
                                                argv[1], argv[2]);
            }
            else
                rc = PyObject_CallFunction (evFunc, "ss", argv[0], argv[1]);
        }
        else
            rc = PyObject_CallFunction (evFunc, "s", argv[0]);
    }
    else
        rc = PyObject_CallFunction (evFunc, NULL);
    
    ret_value = NULL;
    
    /* 
       ugly hack : rc = NULL while 'return weechat.WEECHAT_RC_OK .... 
       because of '#define WEECHAT_RC_OK 0'
    */
    if (rc ==  NULL)
	rc = PyInt_FromLong (0);
    
    if (PyString_Check (rc) && (ret_type == WEECHAT_SCRIPT_EXEC_STRING))
    {
	if (PyString_AsString (rc))
	    ret_value = strdup (PyString_AsString(rc));
	else
	    ret_value = NULL;
	
	Py_XDECREF(rc);
    }
    else if (PyInt_Check (rc) && (ret_type == WEECHAT_SCRIPT_EXEC_INT))
    {
	
	ret_i = malloc (sizeof (*ret_i));
	if (ret_i)
	    *ret_i = (int) PyInt_AsLong(rc);
	ret_value = ret_i;
	
	Py_XDECREF(rc);
    }
    else
    {
	weechat_printf (NULL,
                        weechat_gettext ("%s%s: function \"%s\" must return "
                                         "a valid value"),
                        weechat_prefix ("error"), "python", function);
	/* PyEval_ReleaseThread (python_current_script->interpreter); */
        python_current_script = old_python_current_script;
        if (python_current_script)
            PyThreadState_Swap (python_current_script->interpreter);
	return NULL;
    }
    
    if (ret_value == NULL)
    {
	weechat_printf (NULL,
                        weechat_gettext ("%s%s: not enough memory in "
                                         "function \"%s\""),
                        weechat_prefix ("error"), "python", function);
	/* PyEval_ReleaseThread (python_current_script->interpreter); */
        python_current_script = old_python_current_script;
        if (python_current_script)
            PyThreadState_Swap (python_current_script->interpreter);
	return NULL;
    }
    
    if (PyErr_Occurred ())
        PyErr_Print ();
    
    /* PyEval_ReleaseThread (python_current_script->interpreter); */
    
    python_current_script = old_python_current_script;
    if (python_current_script)
        PyThreadState_Swap (python_current_script->interpreter);
    
    return ret_value;
}

/*
 * weechat_python_output : redirection for stdout and stderr
 */

static PyObject *
weechat_python_output (PyObject *self, PyObject *args)
{
    char *msg, *m, *p;
    
    /* make C compiler happy */
    (void) self;
    
    msg = NULL;

    if (!PyArg_ParseTuple (args, "s", &msg))
    {
        if (strlen(python_buffer_output) > 0)
	{
	    weechat_printf (NULL,
                            weechat_gettext ("%s: stdout/stderr: %s%s"),
                            "python", python_buffer_output, "");
	    python_buffer_output[0] = '\0';
	}
    }
    else 
    {
	m = msg;
	while ((p = strchr (m, '\n')) != NULL)
	{
	    *p = '\0';
	    if (strlen (m) + strlen (python_buffer_output) > 0)
            {
		weechat_printf (NULL,
                                weechat_gettext ("%s: stdout/stderr: %s%s"),
                                "python", python_buffer_output, m);
            }
	    *p = '\n';
	    python_buffer_output[0] = '\0';
	    m = ++p;
	}
	
	if (strlen(m) + strlen(python_buffer_output) > sizeof(python_buffer_output))
	{
	    weechat_printf (NULL,
                            weechat_gettext ("%s: stdout/stderr: %s%s"),
                            "python", python_buffer_output, m);
	    python_buffer_output[0] = '\0';
	}
	else
	    strcat (python_buffer_output, m);
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Outputs subroutines
 */

static
PyMethodDef weechat_python_output_funcs[] = {
    { "write", weechat_python_output, METH_VARARGS, "" },
    { NULL, NULL, 0, NULL }
};

/*
 * weechat_python_load: load a Python script
 */

int
weechat_python_load (char *filename)
{
    char *argv[] = { "__weechat_plugin__" , NULL };
    FILE *fp;
    PyThreadState *python_current_interpreter;
    PyObject *weechat_module, *weechat_outputs, *weechat_dict;
    PyObject *python_path, *path;
    char *w_home, *p_home;
    int len;
    
    if ((fp = fopen (filename, "r")) == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: script \"%s\" not found"),
                        weechat_prefix ("error"), "python", filename);
        return 0;
    }
    
    weechat_printf (NULL,
                    weechat_gettext ("%s: loading script \"%s\""),
                    "python", filename);
    
    python_current_script = NULL;
    
    /* PyEval_AcquireLock (); */
    python_current_interpreter = Py_NewInterpreter ();
    PySys_SetArgv(1, argv);
    
    if (python_current_interpreter == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to create new "
                                         "sub-interpreter"),
                        weechat_prefix ("error"), "python");
        fclose (fp);
        /* PyEval_ReleaseLock (); */
        return 0;
    }
    
    /* PyThreadState_Swap (python_current_interpreter); */
    
    weechat_module = Py_InitModule ("weechat", weechat_python_funcs);

    if ( weechat_module == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to initialize WeeChat "
                                         "module"),
                        weechat_prefix ("error"), "python");
        fclose (fp);
	
        Py_EndInterpreter (python_current_interpreter);
        /* PyEval_ReleaseLock (); */

        return 0;
    }

    /* adding $weechat_dir/python in $PYTHONPATH */    
    python_path = PySys_GetObject ("path");
    w_home = weechat_info_get ("weechat_dir");
    if (w_home)
    {
        len = strlen (w_home) + 1 + strlen("python") + 1;
        p_home = malloc (len);
        if (p_home)
        {
            snprintf (p_home, len, "%s/python", w_home);
            path = PyString_FromString (p_home);
            if (path != NULL)
            {
                PyList_Insert (python_path, 0, path);
                Py_DECREF (path);
            }
            free (p_home);
        }
    }
    
    /* define some constants */
    weechat_dict = PyModule_GetDict(weechat_module);
    PyDict_SetItemString(weechat_dict, "WEECHAT_RC_OK", PyInt_FromLong((long) WEECHAT_RC_OK));
    PyDict_SetItemString(weechat_dict, "WEECHAT_RC_ERROR", PyInt_FromLong((long) WEECHAT_RC_ERROR));
    
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_READ_OK", PyInt_FromLong((long) WEECHAT_CONFIG_READ_OK));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_READ_MEMORY_ERROR", PyInt_FromLong((long) WEECHAT_CONFIG_READ_MEMORY_ERROR));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_READ_FILE_NOT_FOUND", PyInt_FromLong((long) WEECHAT_CONFIG_READ_FILE_NOT_FOUND));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_WRITE_OK", PyInt_FromLong((long) WEECHAT_CONFIG_WRITE_OK));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_WRITE_ERROR", PyInt_FromLong((long) WEECHAT_CONFIG_WRITE_ERROR));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_WRITE_MEMORY_ERROR", PyInt_FromLong((long) WEECHAT_CONFIG_WRITE_MEMORY_ERROR));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_SET_OK_CHANGED", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_SET_OK_CHANGED));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_SET_OK_SAME_VALUE", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_SET_OK_SAME_VALUE));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_SET_ERROR", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_SET_ERROR));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_SET_OPTION_NOT_FOUND", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_SET_OPTION_NOT_FOUND));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_UNSET_OK_NO_RESET", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_UNSET_OK_NO_RESET));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_UNSET_OK_RESET", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_UNSET_OK_RESET));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_UNSET_OK_REMOVED", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_UNSET_OK_REMOVED));
    PyDict_SetItemString(weechat_dict, "WEECHAT_CONFIG_OPTION_UNSET_ERROR", PyInt_FromLong((long) WEECHAT_CONFIG_OPTION_UNSET_ERROR));
    
    PyDict_SetItemString(weechat_dict, "WEECHAT_LIST_POS_SORT", PyString_FromString(WEECHAT_LIST_POS_SORT));
    PyDict_SetItemString(weechat_dict, "WEECHAT_LIST_POS_BEGINNING", PyString_FromString(WEECHAT_LIST_POS_BEGINNING));
    PyDict_SetItemString(weechat_dict, "WEECHAT_LIST_POS_END", PyString_FromString(WEECHAT_LIST_POS_END));
    
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOTLIST_LOW", PyString_FromString(WEECHAT_HOTLIST_LOW));
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOTLIST_MESSAGE", PyString_FromString(WEECHAT_HOTLIST_MESSAGE));
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOTLIST_PRIVATE", PyString_FromString(WEECHAT_HOTLIST_PRIVATE));
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOTLIST_HIGHLIGHT", PyString_FromString(WEECHAT_HOTLIST_HIGHLIGHT));
    
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOOK_SIGNAL_STRING", PyString_FromString(WEECHAT_HOOK_SIGNAL_STRING));
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOOK_SIGNAL_INT", PyString_FromString(WEECHAT_HOOK_SIGNAL_INT));
    PyDict_SetItemString(weechat_dict, "WEECHAT_HOOK_SIGNAL_POINTER", PyString_FromString(WEECHAT_HOOK_SIGNAL_POINTER));
    
    weechat_outputs = Py_InitModule("weechatOutputs", weechat_python_output_funcs);
    if (weechat_outputs == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to redirect stdout and "
                                         "stderr"),
                        weechat_prefix ("error"), "python");
    }
    else
    {
        if (PySys_SetObject("stdout", weechat_outputs) == -1)
        {
            weechat_printf (NULL,
                            weechat_gettext ("%s%s: unable to redirect stdout"),
                            weechat_prefix ("error"), "python");
        }
        if (PySys_SetObject("stderr", weechat_outputs) == -1)
        {
            weechat_printf (NULL,
                            weechat_gettext ("%s%s: unable to redirect stderr"),
                            weechat_prefix ("error"), "python");
        }
    }
    
    python_current_script_filename = filename;
    
    if (PyRun_SimpleFile (fp, filename) != 0)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to parse file \"%s\""),
                        weechat_prefix ("error"), "python", filename);
        fclose (fp);

        if (PyErr_Occurred ())
            PyErr_Print ();
        Py_EndInterpreter (python_current_interpreter);
        /* PyEval_ReleaseLock (); */

        /* if script was registered, removing from list */
        if (python_current_script != NULL)
            script_remove (weechat_python_plugin, &python_scripts,
                    python_current_script);
        return 0;
    }

    if (PyErr_Occurred ())
        PyErr_Print ();

    fclose (fp);
    
    if (python_current_script == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: function \"register\" not "
                                         "found (or failed) in file \"%s\""),
                        weechat_prefix ("error"), "python", filename);
	
        if (PyErr_Occurred ())
            PyErr_Print ();
        Py_EndInterpreter (python_current_interpreter);
        /* PyEval_ReleaseLock (); */

        return 0;
    }
    
    python_current_script->interpreter = (PyThreadState *) python_current_interpreter;
    /* PyEval_ReleaseThread (python_current_script->interpreter); */
    
    return 1;
}

/*
 * weechat_python_load_cb: callback for script_auto_load() function
 */

int
weechat_python_load_cb (void *data, char *filename)
{
    /* make C compiler happy */
    (void) data;
    
    return weechat_python_load (filename);
}

/*
 * weechat_python_unload: unload a Python script
 */

void
weechat_python_unload (struct t_plugin_script *script)
{
    int *r;
    void *interpreter;
    
    weechat_printf (NULL,
                    weechat_gettext ("%s: unloading script \"%s\""),
                    "python", script->name);
    
    if (script->shutdown_func && script->shutdown_func[0])
    {
        r = (int *) weechat_python_exec (script, WEECHAT_SCRIPT_EXEC_INT,
					 script->shutdown_func, NULL);
	if (r)
	    free (r);
    }
    
    interpreter = script->interpreter;
    
    script_remove (weechat_python_plugin, &python_scripts, script);
    
    PyThreadState_Swap (interpreter);
    Py_EndInterpreter (interpreter);
}

/*
 * weechat_python_unload_name: unload a Python script by name
 */

void
weechat_python_unload_name (char *name)
{
    struct t_plugin_script *ptr_script;
    
    ptr_script = script_search (weechat_python_plugin, python_scripts, name);
    if (ptr_script)
    {
        weechat_python_unload (ptr_script);
        weechat_printf (NULL,
                        weechat_gettext ("%s: script \"%s\" unloaded"),
                        "python", name);
    }
    else
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: script \"%s\" not loaded"),
                        weechat_prefix ("error"), "python", name);
    }
}

/*
 * weechat_python_unload_all: unload all Python scripts
 */

void
weechat_python_unload_all ()
{
    while (python_scripts)
    {
        weechat_python_unload (python_scripts);
    }
}

/*
 * weechat_python_cmd: callback for "/python" command
 */

int
weechat_python_command_cb (void *data, struct t_gui_buffer *buffer,
                           int argc, char **argv, char **argv_eol)
{
    char *path_script;
    
    /* make C compiler happy */
    (void) data;
    (void) buffer;

    if (argc == 1)
    {
        script_display_list (weechat_python_plugin, python_scripts,
                             NULL, 0);
    }
    else if (argc == 2)
    {
        if (weechat_strcasecmp (argv[1], "list") == 0)
        {
            script_display_list (weechat_python_plugin, python_scripts,
                                 NULL, 0);
        }
        else if (weechat_strcasecmp (argv[1], "listfull") == 0)
        {
            script_display_list (weechat_python_plugin, python_scripts,
                                 NULL, 1);
        }
        else if (weechat_strcasecmp (argv[1], "autoload") == 0)
        {
            script_auto_load (weechat_python_plugin, &weechat_python_load_cb);
        }
        else if (weechat_strcasecmp (argv[1], "reload") == 0)
        {
            weechat_python_unload_all ();
            script_auto_load (weechat_python_plugin, &weechat_python_load_cb);
        }
        else if (weechat_strcasecmp (argv[1], "unload") == 0)
        {
            weechat_python_unload_all ();
        }
    }
    else
    {
        if (weechat_strcasecmp (argv[1], "list") == 0)
        {
            script_display_list (weechat_python_plugin, python_scripts,
                                 argv_eol[2], 0);
        }
        else if (weechat_strcasecmp (argv[1], "listfull") == 0)
        {
            script_display_list (weechat_python_plugin, python_scripts,
                                 argv_eol[2], 1);
        }
        else if (weechat_strcasecmp (argv[1], "load") == 0)
        {
            /* load Python script */
            path_script = script_search_full_name (weechat_python_plugin,
                                                   argv_eol[2]);
            weechat_python_load ((path_script) ? path_script : argv_eol[2]);
            if (path_script)
                free (path_script);
        }
        else if (weechat_strcasecmp (argv[1], "unload") == 0)
        {
            /* unload Python script */
            weechat_python_unload_name (argv_eol[2]);
        }
        else
        {
            weechat_printf (NULL,
                            weechat_gettext ("%s%s: unknown option for "
                                             "command \"%s\""),
                            weechat_prefix ("error"), "python", "python");
        }
    }
    
    return WEECHAT_RC_OK;
}

/*
 * weechat_python_completion_cb: callback for script completion
 */

int
weechat_python_completion_cb (void *data, char *completion,
                              struct t_gui_buffer *buffer,
                              struct t_weelist *list)
{
    /* make C compiler happy */
    (void) data;
    (void) completion;
    (void) buffer;
    
    script_completion (weechat_python_plugin, list, python_scripts);
    
    return WEECHAT_RC_OK;
}

/*
 * weechat_python_debug_dump_cb: dump Python plugin data in WeeChat log file
 */

int
weechat_python_debug_dump_cb (void *data, char *signal, char *type_data,
                              void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;
    (void) signal_data;
    
    script_print_log (weechat_python_plugin, python_scripts);
    
    return WEECHAT_RC_OK;
}

/*
 * weechat_python_buffer_closed_cb: callback called when a buffer is closed
 */

int
weechat_python_buffer_closed_cb (void *data, char *signal, char *type_data,
                                 void *signal_data)
{
    /* make C compiler happy */
    (void) data;
    (void) signal;
    (void) type_data;
    
    if (signal_data)
        script_remove_buffer_callbacks (python_scripts, signal_data);
    
    return WEECHAT_RC_OK;
}

/*
 * weechat_plugin_init: initialize Python plugin
 */

int
weechat_plugin_init (struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    /* make C compiler happy */
    (void) argc;
    (void) argv;
    
    weechat_python_plugin = plugin;
    
    /* init stdout/stderr buffer */
    python_buffer_output[0] = '\0';
    
    Py_Initialize ();
    if (Py_IsInitialized () == 0)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to launch global "
                                         "interpreter"),
                        weechat_prefix ("error"), "python");
        return WEECHAT_RC_ERROR;
    }
    
    PyEval_InitThreads();
    /* python_mainThreadState = PyThreadState_Swap(NULL); */
    python_mainThreadState = PyEval_SaveThread();
    /* PyEval_ReleaseLock (); */

    if (python_mainThreadState == NULL)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to get current "
                                         "interpreter state"),
                        weechat_prefix ("error"), "python");
        return WEECHAT_RC_ERROR;
    }
    
    script_init (weechat_python_plugin,
                 &weechat_python_command_cb,
                 &weechat_python_completion_cb,
                 &weechat_python_debug_dump_cb,
                 &weechat_python_buffer_closed_cb,
                 &weechat_python_load_cb);
    
    /* init ok */
    return WEECHAT_RC_OK;
}

/*
 * weechat_plugin_end: shutdown Python interface
 */

int
weechat_plugin_end (struct t_weechat_plugin *plugin)
{
    /* make C compiler happy */
    (void) plugin;
    
    /* unload all scripts */
    weechat_python_unload_all ();
    
    /* free Python interpreter */
    if (python_mainThreadState != NULL)
    {
	/* PyEval_AcquireLock (); */
        PyThreadState_Swap (python_mainThreadState);
	/* PyEval_ReleaseLock (); */
        python_mainThreadState = NULL;
    }
    
    Py_Finalize ();
    if (Py_IsInitialized () != 0)
    {
        weechat_printf (NULL,
                        weechat_gettext ("%s%s: unable to free interpreter"),
                        weechat_prefix ("error"), "python");
    }
    
    return WEECHAT_RC_OK;
}
