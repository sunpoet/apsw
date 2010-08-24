/*
  Another Python Sqlite Wrapper

  This wrapper aims to be the minimum necessary layer over SQLite 3
  itself.

  It assumes we are running as 32 bit int with a 64 bit long long type
  available.

  See the accompanying LICENSE file.
*/

/** 

.. module:: apsw
   :synopsis: Python access to SQLite database library

APSW Module
***********

The module is the main interface to SQLite.  Methods and data on the
module have process wide effects.  You can instantiate the
:class:`Connection` and :class:`zeroblob` objects using
:meth:`Connection` and :meth:`zeroblob` respectively.

API Reference
=============
*/

/* Fight with setuptools over ndebug */
#ifdef APSW_NO_NDEBUG
#ifdef NDEBUG
#undef NDEBUG
#endif
#endif

#ifdef APSW_USE_SQLITE_CONFIG
#include APSW_USE_SQLITE_CONFIG
#endif

/* SQLite amalgamation */
#ifdef APSW_USE_SQLITE_AMALGAMATION
/* See SQLite ticket 2554 */
#define SQLITE_API static
#define SQLITE_EXTERN static
#include APSW_USE_SQLITE_AMALGAMATION

/* Fight with SQLite over ndebug */
#ifdef APSW_NO_NDEBUG
#ifdef NDEBUG
#undef NDEBUG
#endif
#endif

#else
/* SQLite 3 headers */
#include "sqlite3.h"
#endif

#if SQLITE_VERSION_NUMBER < 3007002
#error Your SQLite version is too old.  It must be at least 3.7.2
#endif

/* system headers */
#include <assert.h>
#include <stdarg.h>

/* Get the version number */
#include "apswversion.h"

/* Python headers */
#include <Python.h>
#include <pythread.h>
#include "structmember.h"

#ifdef APSW_TESTFIXTURES
/* Fault injection */
#define APSW_FAULT_INJECT(name,good,bad)          \
do {                                              \
  if(APSW_Should_Fault(#name))                    \
    {                                             \
      do { bad ; } while(0);                      \
    }                                             \
  else                                            \
    {                                             \
      do { good ; } while(0);                     \
    }                                             \
 } while(0)

static int APSW_Should_Fault(const char *);

/* Are we Python 2.x (x>=5) and doing 64 bit? - _LP64 is best way I can find as sizeof isn't valid in cpp #if */
#if  PY_VERSION_HEX>=0x02050000 && defined(_LP64) && _LP64
#define APSW_TEST_LARGE_OBJECTS
#endif

#else /* APSW_TESTFIXTURES */
#define APSW_FAULT_INJECT(name,good,bad)        \
  do { good ; } while(0)

#endif

/* The encoding we use with SQLite.  SQLite supports either utf8 or 16
   bit unicode (host byte order).  If the latter is used then all
   functions have "16" appended to their name.  The encoding used also
   affects how strings are stored in the database.  We use utf8 since
   it is more space efficient, and Python can't make its mind up about
   Unicode (it uses 16 or 32 bit unichars and often likes to use Byte
   Order Markers as well). */
#define STRENCODING "utf-8"

/* The module object */
static PyObject *apswmodule;

/* Everything except the module itself is in seperate files */

/* Augment tracebacks */
#include "traceback.c"

/* Make various versions of Python code compatible with each other */
#include "pyutil.c"

/* Exceptions we can raise */
#include "exceptions.c"

/* various utility functions and macros */
#include "util.c"

/* buffer used in statement cache */
#include "apswbuffer.c"

/* The statement cache */
#include "statementcache.c"

/* connections */
#include "connection.c"

/* backup */
#include "backup.c"

/* Zeroblob and blob */
#include "blob.c"

/* cursors */
#include "cursor.c"

/* virtual tables */
#include "vtable.c"

/* virtual file system */
#include "vfs.c"


/* MODULE METHODS */

/** .. method:: sqlitelibversion() -> string

  Returns the version of the SQLite library.  This value is queried at
  run time from the library so if you use shared libraries it will be
  the version in the shared library.

  -* sqlite3_libversion
*/

static PyObject *
getsqliteversion(void)
{
  return MAKESTR(sqlite3_libversion());
}

/** .. method:: apswversion() -> string

  Returns the APSW version.
*/
static PyObject *
getapswversion(void)
{
  return MAKESTR(APSW_VERSION);
}

/** .. method:: enablesharedcache(bool)

  If you use the same :class:`Connection` across threads or use
  multiple :class:`connections <Connection>` accessing the same file,
  then SQLite can `share the cache between them
  <http://www.sqlite.org/sharedcache.html>`_.  It is :ref:`not
  recommended <sharedcache>` that you use this.
  
  -* sqlite3_enable_shared_cache
*/
static PyObject *
enablesharedcache(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int setting,res;
  if(!PyArg_ParseTuple(args, "i:enablesharedcache(boolean)", &setting))
    return NULL;

  APSW_FAULT_INJECT(EnableSharedCacheFail,res=sqlite3_enable_shared_cache(setting),res=SQLITE_NOMEM);
  SET_EXC(res, NULL);

  if(res!=SQLITE_OK)
    return NULL;

  Py_RETURN_NONE;
}

/** .. method:: initialize()

  It is unlikely you will want to call this method as SQLite automatically initializes.

  -* sqlite3_initialize
*/

static PyObject *
initialize(void)
{
  int res;

  res=sqlite3_initialize();
  APSW_FAULT_INJECT(InitializeFail, ,res=SQLITE_NOMEM);
  SET_EXC(res, NULL);

  if(res!=SQLITE_OK)
    return NULL;

  Py_RETURN_NONE;
}

/** .. method:: shutdown()

  It is unlikely you will want to call this method and there is no
  need to do so.  It is a **really** bad idea to call it unless you
  are absolutely sure all :class:`connections <Connection>`,
  :class:`blobs <blob>`, :class:`cursors <Cursor>`, :class:`vfs <VFS>`
  etc have been closed, deleted and garbage collected.

  -* sqlite3_shutdown
*/

static PyObject *
sqliteshutdown(void)
{
  int res;
  
  APSW_FAULT_INJECT(ShutdownFail, res=sqlite3_shutdown(), res=SQLITE_NOMEM);
  SET_EXC(res, NULL);

  if(res!=SQLITE_OK)
    return NULL;

  Py_RETURN_NONE;
}

/** .. method:: config(op[, *args])

  :param op: A `configuration operation <http://sqlite.org/c3ref/c_config_chunkalloc.html>`_
  :param args: Zero or more arguments as appropriate for *op*

  Many operations don't make sense from a Python program.  Only the
  following configuration operations are supported: SQLITE_CONFIG_LOG,
  SQLITE_CONFIG_SINGLETHREAD, SQLITE_CONFIG_MULTITHREAD,
  SQLITE_CONFIG_SERIALIZED and SQLITE_CONFIG_MEMSTATUS.

  See :func:`log` for an example of using SQLITE_CONFIG_LOG.

  -* sqlite3_config
*/

#ifdef EXPERIMENTAL
static PyObject *logger_cb=NULL;

static void 
apsw_logger(void *arg, int errcode, const char *message)
{
  PyGILState_STATE gilstate;
  PyObject *etype=NULL, *evalue=NULL, *etraceback=NULL;
  PyObject *res=NULL;
  PyObject *msgaspystring=NULL;

  gilstate=PyGILState_Ensure();
  assert(arg==logger_cb);
  assert(arg);
  PyErr_Fetch(&etype, &evalue, &etraceback);

  msgaspystring=convertutf8string(message);
  if(msgaspystring)
    res=PyEval_CallFunction(arg, "iO", errcode, msgaspystring);
  if(!res)
    {
      AddTraceBackHere(__FILE__, __LINE__, "Call_Logger",
		       "{s: O, s: i, s: s}", 
		       "logger", arg,
		       "errcode", errcode,
		       "message", message);
      apsw_write_unraiseable(NULL);
    }
  else
    Py_DECREF(res);

  Py_XDECREF(msgaspystring);
  if(etype || evalue || etraceback)
    PyErr_Restore(etype, evalue, etraceback);
  PyGILState_Release(gilstate);
}

static PyObject *
config(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int res, optdup;
  long opt;

  if(PyTuple_GET_SIZE(args)<1 || !PyIntLong_Check(PyTuple_GET_ITEM(args, 0)))
    {
      PyErr_Format(PyExc_TypeError, "There should be at least one argument with the first being a number");
      return NULL;
    }
  opt=PyIntLong_AsLong(PyTuple_GET_ITEM(args,0));
  if(PyErr_Occurred())
    return NULL;

  switch(opt)
    {
    case SQLITE_CONFIG_SINGLETHREAD:
    case SQLITE_CONFIG_MULTITHREAD:
    case SQLITE_CONFIG_SERIALIZED:
      if(!PyArg_ParseTuple(args, "i", &optdup))
        return NULL;
      assert(opt==optdup);
      res=sqlite3_config( (int)opt );
      break;
      
    case SQLITE_CONFIG_MEMSTATUS:
      {
        int boolval;
        if(!PyArg_ParseTuple(args, "ii", &optdup, &boolval))
          return NULL;
        assert(opt==optdup);
        res=sqlite3_config( (int)opt, boolval);
        break;
      }
      
    case SQLITE_CONFIG_LOG:
      {
	PyObject *logger;
	if(!PyArg_ParseTuple(args, "iO", &optdup, &logger))
	  return NULL;
	if(logger==Py_None)
	  {
	    res=sqlite3_config((int)opt, NULL);
	    if(res==SQLITE_OK)
	      Py_CLEAR(logger_cb);
	  }
	else if(!PyCallable_Check(logger))
	  {
	    PyErr_Format(PyExc_TypeError, "Logger should be None or a callable");
	    return NULL;
	  }
	else
	  {
	    res=sqlite3_config((int)opt, apsw_logger, logger);
	    if(res==SQLITE_OK)
	      {
		Py_CLEAR(logger_cb);
		logger_cb=logger;
		Py_INCREF(logger);
	      }
	  }
	break;
      }

    default:
      PyErr_Format(PyExc_TypeError, "Unknown config type %d", (int)opt);
      return NULL;
    }

  SET_EXC(res, NULL);

  if(res!=SQLITE_OK)
    return NULL;

  Py_RETURN_NONE;
}
#endif /* EXPERIMENTAL */

/** .. method:: memoryused() -> int

  Returns the amount of memory SQLite is currently using.

  .. seealso::
    :meth:`status`


  -* sqlite3_memory_used
*/
static PyObject*
memoryused(void)
{
  return PyLong_FromLongLong(sqlite3_memory_used());
}

/** .. method:: memoryhighwater(reset=False) -> int

  Returns the maximum amount of memory SQLite has used.  If *reset* is
  True then the high water mark is reset to the current value.

  .. seealso::

    :meth:`status`

  -* sqlite3_memory_highwater
*/
static PyObject*
memoryhighwater(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int reset=0;

  if(!PyArg_ParseTuple(args, "|i:memoryhighwater(reset=False)", &reset))
    return NULL;

  return PyLong_FromLongLong(sqlite3_memory_highwater(reset));
}


/** .. method:: softheaplimit(bytes)

  Requests SQLite try to keep memory usage below *bytes* bytes.

  -* sqlite3_soft_heap_limit
*/
static PyObject*
softheaplimit(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int limit;

  if(!PyArg_ParseTuple(args, "i", &limit))
    return NULL;

  sqlite3_soft_heap_limit(limit);

  Py_RETURN_NONE;
}

/** .. method:: randomness(bytes)  -> data

  Gets random data from SQLite's random number generator.

  :param bytes: How many bytes to return
  :rtype: (Python 2) string, (Python 3) bytes

  -* sqlite3_randomness
*/
static PyObject*
randomness(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int amount;
  PyObject *bytes;

  if(!PyArg_ParseTuple(args, "i", &amount))
    return NULL;
  if(amount<0)
    {
      PyErr_Format(PyExc_ValueError, "Can't have negative number of bytes");
      return NULL;
    }
  bytes=PyBytes_FromStringAndSize(NULL, amount);
  if(!bytes) return bytes;
  sqlite3_randomness(amount, PyBytes_AS_STRING(bytes));
  return bytes;
}

/** .. method:: releasememory(bytes) -> int

  Requests SQLite try to free *bytes* bytes of memory.  Returns how
  many bytes were freed.

  -* sqlite3_release_memory
*/

static PyObject*
releasememory(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int amount;

  if(!PyArg_ParseTuple(args, "i", &amount))
    return NULL;

  return PyInt_FromLong(sqlite3_release_memory(amount));
}

/** .. method:: status(op, reset=False) -> (int, int)

  Returns current and highwater measurements.

  :param op: A `status parameter <http://sqlite.org/c3ref/c_status_malloc_size.html>`_
  :param reset: If *True* then the highwater is set to the current value
  :returns: A tuple of current value and highwater value
  
  .. seealso::

    * :ref:`Status example <example-status>`

  -* sqlite3_status

*/
static PyObject *
status(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int res, op, current=0, highwater=0, reset=0;

  if(!PyArg_ParseTuple(args, "i|i:status(op, reset=False)", &op, &reset))
    return NULL;

  res=sqlite3_status(op, &current, &highwater, reset);
  SET_EXC(res, NULL);

  if(res!=SQLITE_OK)
    return NULL;

  return Py_BuildValue("(ii)", current, highwater);
}

/** .. method:: vfsnames() -> list(string)

  Returns a list of the currently installed :ref:`vfs <vfs>`.  The first
  item in the list is the default vfs.
*/
static PyObject *
vfsnames(APSW_ARGUNUSED PyObject *self)
{
  PyObject *result=NULL, *str=NULL;
  sqlite3_vfs *vfs=sqlite3_vfs_find(0);

  result=PyList_New(0);
  if(!result) goto error;

  while(vfs)
    {
      APSW_FAULT_INJECT(vfsnamesfails, 
                        str=convertutf8string(vfs->zName),
                        str=PyErr_NoMemory());
      if(!str) goto error;
      if(PyList_Append(result, str)) goto error;
      Py_DECREF(str);
      vfs=vfs->pNext;
    }
  return result;

 error:
  Py_XDECREF(str);
  Py_XDECREF(result);
  return NULL;
}

/** .. method:: exceptionfor(int) -> Exception

  If you would like to raise an exception that corresponds to a
  particular SQLite `error code
  <http://sqlite.org/c3ref/c_abort.html>`_ then call this function.
  It also understands `extended error codes
  <http://sqlite.org/c3ref/c_ioerr_access.html>`_.

  For example to raise `SQLITE_IOERR_ACCESS <http://sqlite.org/c3ref/c_ioerr_access.html>`_::

    raise apsw.exceptionfor(apsw.SQLITE_IOERR_ACCESS)

*/
static PyObject *
getapswexceptionfor(APSW_ARGUNUSED PyObject *self, PyObject *pycode)
{
  int code, i;
  PyObject *result=NULL;

  if(!PyIntLong_Check(pycode))
    {
      PyErr_Format(PyExc_TypeError, "Argument should be an integer");
      return NULL;
    }
  code=PyIntLong_AsLong(pycode);
  if(PyErr_Occurred()) return NULL;

  for(i=0;exc_descriptors[i].name;i++)
    if (exc_descriptors[i].code==(code&0xff))
      {
        result=PyObject_CallObject(exc_descriptors[i].cls, NULL);
        if(!result) return result;
        break;
      }
  if(!result)
    {
      PyErr_Format(PyExc_ValueError, "%d is not a known error code", code);
      return result;
    }

  PyObject_SetAttrString(result, "extendedresult", PyInt_FromLong(code));
  PyObject_SetAttrString(result, "result", PyInt_FromLong(code&0xff));
  return result;
}

/** .. method:: complete(statement) -> bool

  Returns True if the input string comprises one or more complete SQL
  statements by looking for an unquoted trailing semi-colon.

  An example use would be if you were prompting the user for SQL
  statements and needed to know if you had a whole statement, or
  needed to ask for another line::

    statement=raw_input("SQL> ")
    while not apsw.complete(statement):
       more=raw_input("  .. ")
       statement=statement+"\n"+more

  -* sqlite3_complete
*/
static PyObject *
apswcomplete(APSW_ARGUNUSED Connection *self, PyObject *args)
{
  char *statements=NULL;
  int res;

  if(!PyArg_ParseTuple(args, "es:complete(statement)", STRENCODING, &statements))
    return NULL;

  res=sqlite3_complete(statements);

  PyMem_Free(statements);

  if(res)
    {
      Py_INCREF(Py_True);
      return Py_True;
    }
  Py_INCREF(Py_False);
  return Py_False;
}

#if defined(APSW_TESTFIXTURES) && defined(APSW_USE_SQLITE_AMALGAMATION)
/* a routine to reset the random number generator so that we can test xRandomness */
static PyObject *
apsw_test_reset_rng(APSW_ARGUNUSED PyObject *self)
{
  /* See sqlite3PrngResetState in sqlite's random.c */
  GLOBAL(struct sqlite3PrngType, sqlite3Prng).isInit = 0;

  Py_RETURN_NONE;
}
#endif

#ifdef APSW_TESTFIXTURES
/* xGetLastError isn't actually called anywhere by SQLite so add a
   manual way of doing so
   http://www.sqlite.org/cvstrac/tktview?tn=3337 */

static PyObject *
apsw_call_xGetLastError(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  char *vfsname;
  int bufsize;
  PyObject *resultbuffer=NULL;
  sqlite3_vfs *vfs;
  int res=-1;

  if(!PyArg_ParseTuple(args, "esi", STRENCODING, &vfsname, &bufsize))
    return NULL;

  vfs=sqlite3_vfs_find(vfsname);
  if(!vfs) goto finally;

  resultbuffer=PyBytes_FromStringAndSize(NULL, bufsize);
  if(!resultbuffer) goto finally;

  memset(PyBytes_AS_STRING(resultbuffer), 0, PyBytes_GET_SIZE(resultbuffer));

  res=vfs->xGetLastError(vfs, bufsize, PyBytes_AS_STRING(resultbuffer));

 finally:
  if(vfsname)
    PyMem_Free(vfsname);

  return resultbuffer?Py_BuildValue("Ni", resultbuffer, res):NULL;
}

static PyObject *
apsw_fini(APSW_ARGUNUSED PyObject *self)
{
  APSWBuffer_fini();
  Py_XDECREF(tls_errmsg);

  Py_RETURN_NONE;
}
#endif

#ifdef APSW_USE_SQLITE_ASYNCVFS_H

#define SQLITE3ASYNC_API static

#include APSW_USE_SQLITE_ASYNCVFS_H

/** .. method:: async_initialize(parentvfs, makedefault) -> string

  Registers the asyncvfs.  It is safe to call this method multiple
  times, with subsequent calls having no effect.

  :param parentvfs: Name of the vfs to use for I/O operations.  Use an empty string for the default
  :param makedefault: Make the async vfs be the default vfs.
  :returns: A string containg the name of the default vfs.

  Note that this method cannot return string errors as part of
  exceptions, just generic error codes and messages.  For example if
  you specify a non-existent parentvfs then you just get
  :exc:`apsw.SQLError` with the text `error`.  The `source
  <http://www.sqlite.org/src/finfo?name=ext/async/sqlite3async.h>`__
  lists what error codes could be returned.

  .. seealso::

  * :ref:`ext-asyncvfs`
*/
static PyObject *
apsw_async_initialize(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  char *vfsname=NULL;
  int makedefault=0;
  int res;

  if(!PyArg_ParseTuple(args, "esi", STRENCODING, &vfsname, &makedefault))
    return NULL;

  if(!*vfsname)
    {
      PyMem_Free(vfsname);
      vfsname=NULL;
    }

  _PYSQLITE_CALL_V(res=sqlite3async_initialize(vfsname, makedefault));
  PyMem_Free(vfsname);
  if(res!=SQLITE_OK)
    {
      SET_EXC(res, NULL);
      return NULL;
    }

  return convertutf8string(SQLITEASYNC_VFSNAME);
}

/** .. method:: async_shutdown()

  Unregisters the asyncvfs.  It is safe to call this method multiple
  times with subsequent calls having no effect.  It is not recommended
  that you call this method.  It deallocates everything immediately
  making no checks if anything is in use.  If you want to call it then
  only do so once you are absolutely sure that all databases, files,
  vfs, threads and other objects it references have been closed and
  freed.
*/
static PyObject *
apsw_async_shutdown(APSW_ARGUNUSED PyObject *self)
{
  _PYSQLITE_CALL_V(sqlite3async_shutdown());

  Py_RETURN_NONE;
}

/** .. method:: async_control(op[, params])

    :param op: The operation code (eg SQLITE_ASYNC_HALT)
    :param params: Optional additional parameters depending on the opcode.
    :returns: None or additional information requested

    This table summarises the parameters taken and value returned
    depending on the operation.

    +-----------------------------------+--------------+---------+
    | op                                | params       | result  |
    +===================================+==============+=========+
    | SQLITEASYNC_HALT                  | int          | None    |
    +-----------------------------------+--------------+---------+
    | SQLITEASYNC_GET_HALT              |              | int     |
    +-----------------------------------+--------------+---------+
    | SQLITEASYNC_DELAY                 | int          | None    |
    +-----------------------------------+--------------+---------+
    | SQLITEASYNC_GET_DELAY             |              | int     |
    +-----------------------------------+--------------+---------+
    | SQLITEASYNC_LOCKFILES             | int          | None    |
    +-----------------------------------+--------------+---------+
    | SQLITEASYNC_GET_LOCKFILES         |              | int     |
    +-----------------------------------+--------------+---------+
*/
static PyObject *
apsw_async_control(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int op, inparam, outparam,res;

  if(!PyTuple_Check(args) || PyTuple_GET_SIZE(args)<1)
    {
      PyErr_Format(PyExc_TypeError, "Args should be a tuple of at least one item");
      return NULL;
    }
  if(!PyIntLong_Check(PyTuple_GET_ITEM(args, 0)))
    {
      PyErr_Format(PyExc_TypeError, "Arg 0 must be a number");
      return NULL;
    }

  switch(PyIntLong_AsLong(PyTuple_GET_ITEM(args, 0)))
    {
    case SQLITEASYNC_HALT:
    case SQLITEASYNC_DELAY:
    case SQLITEASYNC_LOCKFILES:
      if(!PyArg_ParseTuple(args, "ii", &op, &inparam))
	return NULL;
      _PYSQLITE_CALL_V(res=sqlite3async_control(op, inparam));
      if(res!=SQLITE_OK)
	{
	  SET_EXC(res, NULL);
	  return NULL;
	}
      Py_RETURN_NONE;

    case SQLITEASYNC_GET_HALT:
    case SQLITEASYNC_GET_DELAY:
    case SQLITEASYNC_GET_LOCKFILES:
      op=PyIntLong_AsLong(PyTuple_GET_ITEM(args, 0));
      assert(!PyErr_Occurred());
      _PYSQLITE_CALL_V(res=sqlite3async_control(op, &outparam));
      APSW_FAULT_INJECT(AsyncControlFails,,res=SQLITE_NOMEM);
      if(res!=SQLITE_OK)
	{
	  SET_EXC(res, NULL);
	  return NULL;
	}
      return PyInt_FromLong(outparam);

    default:
      PyErr_Format(PyExc_ValueError, "Unknown operation argument");
      return NULL;
    }
}

/** .. method:: async_run()

  Call this method from a worker thread and it will do all the
  asynchronous I/O.  You can get the function to exit by calling
  :func:`async_control` with a first parameter of :const:`SQLITEASYNC_HALT` and
  a second parameter of :const:`SQLITEASYNC_HALT_NOW`.
*/
static PyObject *
apsw_async_run(APSW_ARGUNUSED PyObject *self)
{
  _PYSQLITE_CALL_V(sqlite3async_run());

  Py_RETURN_NONE;
}


#endif

#ifdef APSW_FORK_CHECKER

/* 
   We want to verify that SQLite objects are not used across forks.
   One way is to modify all calls to SQLite to do the checking but
   this is a pain as well as a performance hit.  Instead we use the
   approach of providing an alternative mutex implementation since
   pretty much every SQLite API call takes and releases a mutex.

   Our diverted functions check the process id on calls and set the
   process id on allocating a mutex.  We have to avoid the checks for
   the static mutexes.  

   This code also doesn't bother with some things like checking malloc
   results.  It is intended to only be used to verify correctness with
   test suites.  The code that sets Python exceptions is also very
   brute force and is likely to cause problems.  That however is a
   good thing - you will really be sure there is a problem!
 */

typedef struct 
{
  pid_t pid;
  sqlite3_mutex *underlying_mutex;
} apsw_mutex;

static apsw_mutex* apsw_mutexes[]=
  {
    NULL, /* not used - fast */
    NULL, /* not used - recursive */
    NULL, /* from this point on corresponds to the various static mutexes */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };

static sqlite3_mutex_methods apsw_orig_mutex_methods;

static int
apsw_xMutexInit(void)
{
  return apsw_orig_mutex_methods.xMutexInit();
}

static int
apsw_xMutexEnd(void)
{
  return apsw_orig_mutex_methods.xMutexEnd();
}

static sqlite3_mutex*
apsw_xMutexAlloc(int which)
{
  switch(which)
    {
    case SQLITE_MUTEX_FAST:
    case SQLITE_MUTEX_RECURSIVE:
      {
	apsw_mutex *am;
	sqlite3_mutex *m=apsw_orig_mutex_methods.xMutexAlloc(which);
	
	if(!m) return m;

	am=malloc(sizeof(apsw_mutex));
	am->pid=getpid();
	am->underlying_mutex=m;
	return (sqlite3_mutex*)am;
      }
    default:
      /* verify we have space */
      assert(which<sizeof(apsw_mutexes)/sizeof(apsw_mutexes[0]));
      /* fill in if missing */
      if(!apsw_mutexes[which])
	{
	  apsw_mutexes[which]=malloc(sizeof(apsw_mutex));
	  apsw_mutexes[which]->pid=0;
	  apsw_mutexes[which]->underlying_mutex=apsw_orig_mutex_methods.xMutexAlloc(which);
	}
      return (sqlite3_mutex*)apsw_mutexes[which];
    }
}

static int
apsw_check_mutex(apsw_mutex *am)
{
  if(am->pid && am->pid!=getpid())
    {
      PyGILState_STATE gilstate;
      gilstate=PyGILState_Ensure();
      PyErr_Format(ExcForkingViolation, "SQLite object allocated in one process is being used in another (across a fork)");
      apsw_write_unraiseable(NULL);
      PyErr_Format(ExcForkingViolation, "SQLite object allocated in one process is being used in another (across a fork)");
      PyGILState_Release(gilstate);
      return SQLITE_MISUSE;
    }
  return SQLITE_OK;
}

static void
apsw_xMutexFree(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  apsw_check_mutex(am);
  apsw_orig_mutex_methods.xMutexFree(am->underlying_mutex);
}

static void
apsw_xMutexEnter(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  apsw_check_mutex(am);
  apsw_orig_mutex_methods.xMutexEnter(am->underlying_mutex);
}

static int
apsw_xMutexTry(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  if(apsw_check_mutex(am)) return SQLITE_MISUSE;
  return apsw_orig_mutex_methods.xMutexTry(am->underlying_mutex);
}

static void
apsw_xMutexLeave(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  apsw_check_mutex(am);
  apsw_orig_mutex_methods.xMutexLeave(am->underlying_mutex);
}

#ifdef SQLITE_DEBUG
static int
apsw_xMutexHeld(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  apsw_check_mutex(am);
  return apsw_orig_mutex_methods.xMutexHeld(am->underlying_mutex);
}

static int
apsw_xMutexNotheld(sqlite3_mutex *mutex)
{
  apsw_mutex* am=(apsw_mutex*)mutex;
  apsw_check_mutex(am);
  return apsw_orig_mutex_methods.xMutexNotheld(am->underlying_mutex);
}
#endif

static sqlite3_mutex_methods apsw_mutex_methods=
  {
    apsw_xMutexInit,
    apsw_xMutexEnd,
    apsw_xMutexAlloc,
    apsw_xMutexFree,
    apsw_xMutexEnter,
    apsw_xMutexTry,
    apsw_xMutexLeave,
#ifdef SQLITE_DEBUG
    apsw_xMutexHeld,
    apsw_xMutexNotheld
#else
    0,
    0
#endif
  };


/** .. method:: fork_checker()

  **Note** This method is not available on Windows as it does not
  support the fork system call.

  SQLite does not allow the use of database connections across `forked
  <http://en.wikipedia.org/wiki/Fork_(operating_system)>`__ processes
  (see the `SQLite FAQ Q6 <http://www.sqlite.org/faq.html#q6>`__).
  (Forking creates a child process that is a duplicate of the parent
  including the state of all data structures in the program.  If you
  do this to SQLite then parent and child would both consider
  themselves owners of open databases and silently corrupt each
  other's work and interfere with each other's locks.)

  One example of how you may end up using fork is if you use the
  `multiprocessing module
  <http://docs.python.org/library/multiprocessing.html>`__ which uses
  fork to make child processes.

  If you do use fork or multiprocessing on a platform that supports
  fork then you **must** ensure database connections and their objects
  (cursors, backup, blobs etc) are not used in the parent process, or
  are all closed before calling fork or starting a `Process
  <http://docs.python.org/library/multiprocessing.html#process-and-exceptions>`__.
  (Note you must call close to ensure the underlying SQLite objects
  are closed.  It is also a good idea to call `gc.collect(2)
  <http://docs.python.org/library/gc.html#gc.collect>`__ to ensure
  anything you may have missed is also deallocated.)

  Once you run this method, extra checking code is inserted into
  SQLite's mutex operations (at a very small performance penalty) that
  verifies objects are not used across processes.  You will get a
  :exc:`ForkingViolationError` if you do so.  Note that due to the way
  Python's internals work, the exception will be delivered to
  `sys.excepthook` in addition to the normal exception mechanisms and
  may be reported by Python after the line where the issue actually
  arose.  (Destructors of objects you didn't close also run between
  lines.)

  You should only call this method as the first line after importing
  APSW, as it has to shutdown and re-initialize SQLite.  If you have
  any SQLite objects already allocated when calling the method then
  the program will later crash.  The recommended use is to use the fork
  checking as part of your test suite.
*/
static PyObject *
apsw_fork_checker(APSW_ARGUNUSED PyObject *self)
{
  int rc;

  /* ignore multiple attempts to use this routine */
  if(apsw_orig_mutex_methods.xMutexInit) goto ok;


  /* Ensure mutex methods available and installed */
  rc=sqlite3_initialize();
  if(rc) goto fail;

  /* then do a shutdown as we can't get or change mutex while sqlite is running */
  rc=sqlite3_shutdown();
  if(rc) goto fail;
  
  rc=sqlite3_config(SQLITE_CONFIG_GETMUTEX, &apsw_orig_mutex_methods);
  if(rc) goto fail;
  
  rc=sqlite3_config(SQLITE_CONFIG_MUTEX, &apsw_mutex_methods);
  if(rc) goto fail;

  /* start back up again */
  rc=sqlite3_initialize();
  if(rc) goto fail;

 ok:
  Py_RETURN_NONE;
  
 fail:
  assert(rc!=SQLITE_OK);
  SET_EXC(rc, NULL);
  return NULL;
}
#endif

/** .. attribute:: compile_options

    A tuple of the options used to compile SQLite.  For example it
    will be something like this::

        ('ENABLE_LOCKING_STYLE=0', 'TEMP_STORE=1', 'THREADSAFE=1')

    -* sqlite3_compileoption_get
*/
static PyObject*
get_compile_options(void)
{
  int i, count=0;
  const char *opt;
  PyObject *tmpstring;
  PyObject *res=0;

  for(i=0;;i++)
    {
      opt=sqlite3_compileoption_get(i); /* No PYSQLITE_CALL needed */
      if(!opt)
	break;
    }
  count=i;

  res=PyTuple_New(count);
  if(!res) goto fail;
  for(i=0;i<count;i++)
    {
      opt=sqlite3_compileoption_get(i); /* No PYSQLITE_CALL needed */
      assert(opt);
      tmpstring=MAKESTR(opt);
      if(!tmpstring) goto fail;
      PyTuple_SET_ITEM(res, i, tmpstring);
    }

  return res;
 fail:
  Py_XDECREF(res);
  return NULL;
}


/** .. method:: format_sql_value(value) -> string

  Returns a Python string (unicode) representing the supplied value in
  SQL syntax.  Python 2 note: You must supply unicode strings not
  plain strings.

*/
static PyObject*
formatsqlvalue(APSW_ARGUNUSED PyObject *self, PyObject *value)
{
  /* NULL/None */
  if(value==Py_None)
    {
      static PyObject *nullstr;
      if(!nullstr) nullstr=PyObject_Unicode(MAKESTR("NULL"));
      Py_INCREF(nullstr);
      return nullstr;
    }
  /* Integer/Long/Float */
  if(PyIntLong_Check(value)  /* ::TODO:: verify L is not appended in py 2.3 and similar vintage */
     || PyFloat_Check(value))
    return PyObject_Unicode(value);
#if PY_MAJOR_VERSION<3
  /* We don't support plain strings only unicode */
  if(PyString_Check(value))
    {
      PyErr_Format(PyExc_TypeError, "Old plain strings not supported - use unicode");
      return NULL;
    }
#endif
  /* Unicode */
  if(PyUnicode_Check(value))
    {
      /* We optimize for the default case of there being no nuls or single quotes */
      PyObject *unires;
      Py_UNICODE *res;
      Py_ssize_t left;
      unires=PyUnicode_FromUnicode(NULL, PyUnicode_GET_SIZE(value)+2);
      if(!unires) return NULL;
      res=PyUnicode_AS_UNICODE(unires);
      *res++='\'';
      memcpy(res, PyUnicode_AS_UNICODE(value), PyUnicode_GET_DATA_SIZE(value));
      res+=PyUnicode_GET_SIZE(value);
      *res++='\'';
      /* Now look for nuls and single quotes */
      res=PyUnicode_AS_UNICODE(unires)+1;
      left=PyUnicode_GET_SIZE(value);
      for(;left;left--,res++)
	{
	  if(*res=='\'' || *res==0)
	    {
	      /* we add one char for ' and 10 for null */
	      const int moveamount=*res=='\''?1:10;
	      int retval;
	      APSW_FAULT_INJECT(FormatSQLValueResizeFails,
				retval=PyUnicode_Resize(&unires, PyUnicode_GET_SIZE(unires)+moveamount),
				retval=PyUnicode_Resize(&unires, -17)
				);
	      if(retval==-1)
		{
		  Py_DECREF(unires);
		  return NULL;
		}
	      res=PyUnicode_AS_UNICODE(unires)+(PyUnicode_GET_SIZE(unires)-left-moveamount-1);
	      memmove(res+moveamount, res, sizeof(Py_UNICODE)*(left+1));
	      if(*res==0)
		{
		  *res++='\'';
		  *res++='|'; *res++='|'; 
		  *res++='X'; *res++='\''; *res++='0'; *res++='0'; *res++='\''; 
		  *res++='|'; *res++='|'; 
		  *res='\'';
		}
	      else
		res++;
	    }
	}
      return unires;
    }
  /* Blob */
  if(
#if PY_MAJOR_VERSION<3
     PyBuffer_Check(value)
#else
     PyBytes_Check(value)
#endif
     )
    {
      const unsigned char *buffer;
      Py_ssize_t buflen;
      int asrb;
      PyObject *unires;
      Py_UNICODE *res;
#define _HEXDIGITS 

      asrb=PyObject_AsReadBuffer(value, (const void**)&buffer, &buflen);
      APSW_FAULT_INJECT(FormatSQLValueAsReadBufferFails,
			,
			(PyErr_NoMemory(), asrb=-1));
      if(asrb!=0)
	return NULL;
      /* 3 is X, ', '  */
      APSW_FAULT_INJECT(FormatSQLValuePyUnicodeFromUnicodeFails,
			unires=PyUnicode_FromUnicode(NULL, buflen*2+3),
			unires=PyErr_NoMemory());
      if(!unires) 
	return NULL;
      res=PyUnicode_AS_UNICODE(unires);
      *res++='X';
      *res++='\'';
      /* About the billionth time I have written a hex conversion routine */
      for(;buflen;buflen--)
	{
	  *res++="0123456789ABCDEF"[(*buffer)>>4];
	  *res++="0123456789ABCDEF"[(*buffer++)&0x0f];
	}
      *res++='\'';
      return unires;
    }

  PyErr_Format(PyExc_TypeError, "Unsupported type");
  return NULL;
}

/** .. automethod:: main()

  Sphinx automethod is too stupid, so this text is replaced by
  my code with the actual docstring from tools.py:main().
*/

#ifdef EXPERIMENTAL
/** .. method:: log(level, message)

    Calls the SQLite logging interface.  Note that you must format the
    message before passing it to this method::

        apsw.log(apsw.SQLITE_NOMEM, "Need %d bytes of memory" % (1234,))

    To set your own logger use::

        def handler(errcode, message):
           print errcode, message
        apsw.config(apsw.SQLITE_CONFIG_LOG, handler)

    *handler* will be called with two arguments.  The first will be a
    numeric error code and the second will be a message string.  Note
    that the handler has to be set before any other calls to SQLite.
    Once SQLite is initialised you cannot change the logger - a
    :exc:`MisuseError` will happen (this restriction is in SQLite not
    APSW).

    -* sqlite3_log
 */
static PyObject *
apsw_log(APSW_ARGUNUSED PyObject *self, PyObject *args)
{
  int level;
  char *message;
  if(!PyArg_ParseTuple(args, "ies", &level, STRENCODING, &message))
    return NULL;
  sqlite3_log(level, "%s", message); /* PYSQLITE_CALL not needed */
  PyMem_Free(message);
  Py_RETURN_NONE;
}
#endif


static PyMethodDef module_methods[] = {
  {"sqlitelibversion", (PyCFunction)getsqliteversion, METH_NOARGS,
   "Return the version of the SQLite library"},
  {"apswversion", (PyCFunction)getapswversion, METH_NOARGS,
   "Return the version of the APSW wrapper"},
  {"vfsnames", (PyCFunction)vfsnames, METH_NOARGS,
   "Returns list of vfs names"},
  {"enablesharedcache", (PyCFunction)enablesharedcache, METH_VARARGS,
   "Sets shared cache semantics for this thread"},
  {"initialize", (PyCFunction)initialize, METH_NOARGS,
   "Initialize SQLite library"},
  {"shutdown", (PyCFunction)sqliteshutdown, METH_NOARGS,
   "Shutdown SQLite library"},
  {"format_sql_value", (PyCFunction)formatsqlvalue, METH_O,
   "Formats a SQL value as a string"},
#ifdef EXPERIMENTAL
  {"config", (PyCFunction)config, METH_VARARGS,
   "Calls sqlite3_config"},
  {"log", (PyCFunction)apsw_log, METH_VARARGS,
   "Calls sqlite3_log"},
#endif
  {"memoryused", (PyCFunction)memoryused, METH_NOARGS,
   "Current SQLite memory in use"},
  {"memoryhighwater", (PyCFunction)memoryhighwater, METH_VARARGS,
   "Most amount of memory used"},
  {"status", (PyCFunction)status, METH_VARARGS,
   "Gets various SQLite counters"},
  {"softheaplimit", (PyCFunction)softheaplimit, METH_VARARGS,
   "Sets soft limit on SQLite memory usage"},
  {"releasememory", (PyCFunction)releasememory, METH_VARARGS,
   "Attempts to free specified amount of memory"},
  {"randomness", (PyCFunction)randomness, METH_VARARGS,
   "Obtains random bytes"},
  {"exceptionfor", (PyCFunction)getapswexceptionfor, METH_O,
   "Returns exception instance corresponding to supplied sqlite error code"},
  {"complete", (PyCFunction)apswcomplete, METH_VARARGS,
   "Tests if a complete SQLite statement has been supplied (ie ends with ;)"},
#if defined(APSW_TESTFIXTURES) && defined(APSW_USE_SQLITE_AMALGAMATION)
  {"test_reset_rng", (PyCFunction)apsw_test_reset_rng, METH_NOARGS,
   "Resets random number generator so we can test vfs xRandomness"},
#endif
#ifdef APSW_TESTFIXTURES
  {"test_call_xGetLastError", (PyCFunction)apsw_call_xGetLastError, METH_VARARGS,
   "Calls xGetLastError routine"},
  {"_fini", (PyCFunction)apsw_fini, METH_NOARGS,
   "Frees all caches and recycle lists"},
#endif
#ifdef APSW_USE_SQLITE_ASYNCVFS_H
  {"async_initialize", (PyCFunction)apsw_async_initialize, METH_VARARGS,
   "Initializes the asyncvfs extension"},
  {"async_shutdown", (PyCFunction)apsw_async_shutdown, METH_NOARGS,
   "Frees all asyncvfs objects"},
  {"async_control", (PyCFunction)apsw_async_control, METH_VARARGS,
   "Control operation of asyncvfs"},
  {"async_run", (PyCFunction)apsw_async_run, METH_NOARGS,
   "Does the background async I/O"},
#endif
#ifdef APSW_FORK_CHECKER
  {"fork_checker", (PyCFunction)apsw_fork_checker, METH_NOARGS,
   "Installs fork checking code"},
#endif
  {0, 0, 0, 0}  /* Sentinel */
};


static void add_shell(PyObject *module);


#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef apswmoduledef={
  PyModuleDef_HEAD_INIT,
  "apsw", 
  NULL,
  -1,
  module_methods,
  0,
  0,
  0,
  0
};
#endif


PyMODINIT_FUNC
#if PY_MAJOR_VERSION < 3
initapsw(void) 
#else
PyInit_apsw(void)
#endif
{
    PyObject *m=NULL;
    PyObject *thedict=NULL;
    const char *mapping_name=NULL;
    PyObject *hooks;
    unsigned int i;

    assert(sizeof(int)==4);             /* we expect 32 bit ints */
    assert(sizeof(long long)==8);             /* we expect 64 bit long long */

    /* Check SQLite was compiled with thread safety */
    if(!sqlite3_threadsafe())
      {
        PyErr_Format(PyExc_EnvironmentError, "SQLite was compiled without thread safety and cannot be used.");
        goto fail;
      }

    if (PyType_Ready(&ConnectionType) < 0
        || PyType_Ready(&APSWCursorType) < 0
        || PyType_Ready(&ZeroBlobBindType) <0
        || PyType_Ready(&APSWBlobType) <0
        || PyType_Ready(&APSWVFSType) <0
        || PyType_Ready(&APSWVFSFileType) <0
        || PyType_Ready(&APSWStatementType) <0
        || PyType_Ready(&APSWBufferType) <0
        || PyType_Ready(&FunctionCBInfoType) <0
#ifdef EXPERIMENTAL
        || PyType_Ready(&APSWBackupType) <0
#endif
        )
      goto fail;

    /* ensure threads are available */
    PyEval_InitThreads();

#if PY_MAJOR_VERSION < 3
    m = apswmodule = Py_InitModule3("apsw", module_methods,
                       "Another Python SQLite Wrapper.");
#else
    m = apswmodule = PyModule_Create(&apswmoduledef);
#endif

    if (m == NULL)  goto fail;

    if(init_exceptions(m)) goto fail;

    Py_INCREF(&ConnectionType);
    PyModule_AddObject(m, "Connection", (PyObject *)&ConnectionType);
    
    /* we don't add cursor, blob or backup to the module since users shouldn't be able to instantiate them directly */
    
    Py_INCREF(&ZeroBlobBindType);
    PyModule_AddObject(m, "zeroblob", (PyObject *)&ZeroBlobBindType);

    Py_INCREF(&APSWVFSType);
    PyModule_AddObject(m, "VFS", (PyObject*)&APSWVFSType);
    Py_INCREF(&APSWVFSFileType);
    PyModule_AddObject(m, "VFSFile", (PyObject*)&APSWVFSFileType);
    
    /** .. attribute:: connection_hooks

       The purpose of the hooks is to allow the easy registration of
       :meth:`functions <Connection.createscalarfunction>`,
       :ref:`virtual tables <virtualtables>` or similar items with
       each :class:`Connection` as it is created. The default value is an empty
       list. Whenever a Connection is created, each item in
       apsw.connection_hooks is invoked with a single parameter being
       the new Connection object. If the hook raises an exception then
       the creation of the Connection fails.

       If you wanted to store your own defined functions in the
       database then you could define a hook that looked in the
       relevant tables, got the Python text and turned it into the
       functions.
    */
    hooks=PyList_New(0);
    if(!hooks) goto fail;
    PyModule_AddObject(m, "connection_hooks", hooks);

    /** .. data:: SQLITE_VERSION_NUMBER

    The integer version number of SQLite that APSW was compiled
    against.  For example SQLite 3.6.4 will have the value *3006004*.
    This number may be different than the actual library in use if the
    library is shared and has been updated.  Call
    :meth:`sqlitelibversion` to get the actual library version.

    */
    PyModule_AddIntConstant(m, "SQLITE_VERSION_NUMBER", SQLITE_VERSION_NUMBER);
    

    /** .. attribute:: using_amalgamation

    If True then `SQLite amalgamation
    <http://www.sqlite.org/cvstrac/wiki?p=TheAmalgamation>`__ is in
    use (statically compiled into APSW).  Using the amalgamation means
    that SQLite shared libraries are not used and will not affect your
    code.
    
    */

#ifdef APSW_USE_SQLITE_AMALGAMATION
    Py_INCREF(Py_True);
    PyModule_AddObject(m, "using_amalgamation", Py_True);
#else
    Py_INCREF(Py_False);
    PyModule_AddObject(m, "using_amalgamation", Py_False);
#endif

    /** 

.. _sqliteconstants:

SQLite constants
================

SQLite has `many constants
<http://sqlite.org/c3ref/constlist.html>`_ used in various
interfaces.  To use a constant such as :const:`SQLITE_OK`, just
use ``apsw.SQLITE_OK``.

The same values can be used in different contexts. For example
:const:`SQLITE_OK` and :const:`SQLITE_CREATE_INDEX` both have a value
of zero. For each group of constants there is also a mapping (dict)
available that you can supply a string to and get the corresponding
numeric value, or supply a numeric value and get the corresponding
string. These can help improve diagnostics/logging, calling other
modules etc. For example::

      apsw.mapping_authorizer_function["SQLITE_READ"]=20
      apsw.mapping_authorizer_function[20]="SQLITE_READ"


    */

    /* add in some constants and also put them in a corresponding mapping dictionary */

    {

    /* sentinel should be a number that doesn't exist */
#define SENTINEL -786343
#define DICT(n) {n, SENTINEL}
#define END {NULL, 0}
#define ADDINT(n) {#n, n}

    static const
    struct { const char *name; int value; } integers[]={
      DICT("mapping_authorizer_return"),
      ADDINT(SQLITE_DENY),
      ADDINT(SQLITE_IGNORE),
      ADDINT(SQLITE_OK),
      END,
      
      DICT("mapping_authorizer_function"),
      ADDINT(SQLITE_CREATE_INDEX),
      ADDINT(SQLITE_CREATE_TABLE),
      ADDINT(SQLITE_CREATE_TEMP_INDEX),
      ADDINT(SQLITE_CREATE_TEMP_TABLE),
      ADDINT(SQLITE_CREATE_TEMP_TRIGGER),
      ADDINT(SQLITE_CREATE_TEMP_VIEW),
      ADDINT(SQLITE_CREATE_TRIGGER),
      ADDINT(SQLITE_CREATE_VIEW),
      ADDINT(SQLITE_DELETE),
      ADDINT(SQLITE_DROP_INDEX),
      ADDINT(SQLITE_DROP_TABLE),
      ADDINT(SQLITE_DROP_TEMP_INDEX),
      ADDINT(SQLITE_DROP_TEMP_TABLE),
      ADDINT(SQLITE_DROP_TEMP_TRIGGER),
      ADDINT(SQLITE_DROP_TEMP_VIEW),
      ADDINT(SQLITE_DROP_TRIGGER),
      ADDINT(SQLITE_DROP_VIEW),
      ADDINT(SQLITE_INSERT),
      ADDINT(SQLITE_PRAGMA),
      ADDINT(SQLITE_READ),
      ADDINT(SQLITE_SELECT),
      ADDINT(SQLITE_TRANSACTION),
      ADDINT(SQLITE_UPDATE),
      ADDINT(SQLITE_ATTACH),
      ADDINT(SQLITE_DETACH),
      ADDINT(SQLITE_ALTER_TABLE),
      ADDINT(SQLITE_REINDEX),
      ADDINT(SQLITE_COPY),
      ADDINT(SQLITE_ANALYZE),
      ADDINT(SQLITE_CREATE_VTABLE),
      ADDINT(SQLITE_DROP_VTABLE),
      ADDINT(SQLITE_FUNCTION),
      ADDINT(SQLITE_SAVEPOINT),
      END,

      /* vtable best index constraints */
      DICT("mapping_bestindex_constraints"),
      ADDINT(SQLITE_INDEX_CONSTRAINT_EQ),
      ADDINT(SQLITE_INDEX_CONSTRAINT_GT),
      ADDINT(SQLITE_INDEX_CONSTRAINT_LE),
      ADDINT(SQLITE_INDEX_CONSTRAINT_LT),
      ADDINT(SQLITE_INDEX_CONSTRAINT_GE),
      ADDINT(SQLITE_INDEX_CONSTRAINT_MATCH),
      END,

      /* extendended result codes */
      DICT("mapping_extended_result_codes"),
      ADDINT(SQLITE_IOERR_READ),
      ADDINT(SQLITE_IOERR_SHORT_READ),
      ADDINT(SQLITE_IOERR_WRITE),
      ADDINT(SQLITE_IOERR_FSYNC),
      ADDINT(SQLITE_IOERR_DIR_FSYNC),
      ADDINT(SQLITE_IOERR_TRUNCATE),
      ADDINT(SQLITE_IOERR_FSTAT),
      ADDINT(SQLITE_IOERR_UNLOCK),
      ADDINT(SQLITE_IOERR_RDLOCK),
      ADDINT(SQLITE_IOERR_DELETE),
      ADDINT(SQLITE_IOERR_BLOCKED),
      ADDINT(SQLITE_IOERR_NOMEM),
      ADDINT(SQLITE_IOERR_ACCESS),
      ADDINT(SQLITE_IOERR_CHECKRESERVEDLOCK),
      ADDINT(SQLITE_IOERR_LOCK),
      ADDINT(SQLITE_IOERR_CLOSE),
      ADDINT(SQLITE_IOERR_DIR_CLOSE),
      ADDINT(SQLITE_LOCKED_SHAREDCACHE),
      ADDINT(SQLITE_BUSY_RECOVERY),
      ADDINT(SQLITE_CANTOPEN_NOTEMPDIR),
      ADDINT(SQLITE_IOERR_SHMOPEN),
      ADDINT(SQLITE_IOERR_SHMSIZE),
      ADDINT(SQLITE_IOERR_SHMLOCK),
      END,

      /* error codes */
      DICT("mapping_result_codes"),
      ADDINT(SQLITE_OK),
      ADDINT(SQLITE_ERROR),
      ADDINT(SQLITE_INTERNAL),
      ADDINT(SQLITE_PERM),
      ADDINT(SQLITE_ABORT),
      ADDINT(SQLITE_BUSY),
      ADDINT(SQLITE_LOCKED),
      ADDINT(SQLITE_NOMEM),
      ADDINT(SQLITE_READONLY),
      ADDINT(SQLITE_INTERRUPT),
      ADDINT(SQLITE_IOERR),
      ADDINT(SQLITE_CORRUPT),
      ADDINT(SQLITE_FULL),
      ADDINT(SQLITE_CANTOPEN),
      ADDINT(SQLITE_PROTOCOL),
      ADDINT(SQLITE_EMPTY),
      ADDINT(SQLITE_SCHEMA),
      ADDINT(SQLITE_CONSTRAINT),
      ADDINT(SQLITE_MISMATCH),
      ADDINT(SQLITE_MISUSE),
      ADDINT(SQLITE_NOLFS),
      ADDINT(SQLITE_AUTH),
      ADDINT(SQLITE_FORMAT),
      ADDINT(SQLITE_RANGE),
      ADDINT(SQLITE_NOTADB),
      ADDINT(SQLITE_NOTFOUND),
      ADDINT(SQLITE_TOOBIG),
      END,

      /* open flags */
      DICT("mapping_open_flags"),
      ADDINT(SQLITE_OPEN_READONLY),
      ADDINT(SQLITE_OPEN_READWRITE),
      ADDINT(SQLITE_OPEN_CREATE),
      ADDINT(SQLITE_OPEN_DELETEONCLOSE),
      ADDINT(SQLITE_OPEN_EXCLUSIVE),
      ADDINT(SQLITE_OPEN_MAIN_DB),
      ADDINT(SQLITE_OPEN_TEMP_DB),
      ADDINT(SQLITE_OPEN_TRANSIENT_DB),
      ADDINT(SQLITE_OPEN_MAIN_JOURNAL),
      ADDINT(SQLITE_OPEN_TEMP_JOURNAL),
      ADDINT(SQLITE_OPEN_SUBJOURNAL),
      ADDINT(SQLITE_OPEN_MASTER_JOURNAL),
      ADDINT(SQLITE_OPEN_NOMUTEX),
      ADDINT(SQLITE_OPEN_FULLMUTEX),
      ADDINT(SQLITE_OPEN_PRIVATECACHE),
      ADDINT(SQLITE_OPEN_SHAREDCACHE),
      ADDINT(SQLITE_OPEN_AUTOPROXY),
      ADDINT(SQLITE_OPEN_WAL),
      END,

      /* limits */
      DICT("mapping_limits"),
      ADDINT(SQLITE_LIMIT_LENGTH),
      ADDINT(SQLITE_LIMIT_SQL_LENGTH),
      ADDINT(SQLITE_LIMIT_COLUMN),
      ADDINT(SQLITE_LIMIT_EXPR_DEPTH),
      ADDINT(SQLITE_LIMIT_COMPOUND_SELECT),
      ADDINT(SQLITE_LIMIT_VDBE_OP),
      ADDINT(SQLITE_LIMIT_FUNCTION_ARG),
      ADDINT(SQLITE_LIMIT_ATTACHED),
      ADDINT(SQLITE_LIMIT_LIKE_PATTERN_LENGTH),
      ADDINT(SQLITE_LIMIT_VARIABLE_NUMBER),
      ADDINT(SQLITE_LIMIT_TRIGGER_DEPTH),
      /* We don't include the MAX limits - see http://code.google.com/p/apsw/issues/detail?id=17 */
      END,

      DICT("mapping_config"),
      ADDINT(SQLITE_CONFIG_SINGLETHREAD),
      ADDINT(SQLITE_CONFIG_MULTITHREAD),
      ADDINT(SQLITE_CONFIG_SERIALIZED),
      ADDINT(SQLITE_CONFIG_MALLOC),
      ADDINT(SQLITE_CONFIG_GETMALLOC),
      ADDINT(SQLITE_CONFIG_SCRATCH),
      ADDINT(SQLITE_CONFIG_PAGECACHE),
      ADDINT(SQLITE_CONFIG_HEAP),
      ADDINT(SQLITE_CONFIG_MEMSTATUS),
      ADDINT(SQLITE_CONFIG_MUTEX),
      ADDINT(SQLITE_CONFIG_GETMUTEX),
      ADDINT(SQLITE_CONFIG_LOOKASIDE),
      ADDINT(SQLITE_CONFIG_LOG),
      ADDINT(SQLITE_CONFIG_GETPCACHE),
      ADDINT(SQLITE_CONFIG_PCACHE),
      END,

      DICT("mapping_db_config"),
      ADDINT(SQLITE_DBCONFIG_LOOKASIDE),
      END,

      DICT("mapping_status"),
      ADDINT(SQLITE_STATUS_MEMORY_USED),
      ADDINT(SQLITE_STATUS_PAGECACHE_USED),
      ADDINT(SQLITE_STATUS_PAGECACHE_OVERFLOW),
      ADDINT(SQLITE_STATUS_SCRATCH_USED),
      ADDINT(SQLITE_STATUS_SCRATCH_OVERFLOW),
      ADDINT(SQLITE_STATUS_MALLOC_SIZE),
      ADDINT(SQLITE_STATUS_PARSER_STACK),
      ADDINT(SQLITE_STATUS_PAGECACHE_SIZE),
      ADDINT(SQLITE_STATUS_SCRATCH_SIZE),
      ADDINT(SQLITE_STATUS_MALLOC_COUNT),
      END,

      DICT("mapping_db_status"),
      ADDINT(SQLITE_DBSTATUS_LOOKASIDE_USED),
      ADDINT(SQLITE_DBSTATUS_CACHE_USED),
      ADDINT(SQLITE_DBSTATUS_MAX),
      ADDINT(SQLITE_DBSTATUS_SCHEMA_USED),
      ADDINT(SQLITE_DBSTATUS_STMT_USED),
      END,

      DICT("mapping_locking_level"),
      ADDINT(SQLITE_LOCK_NONE),
      ADDINT(SQLITE_LOCK_SHARED),
      ADDINT(SQLITE_LOCK_RESERVED),
      ADDINT(SQLITE_LOCK_PENDING),
      ADDINT(SQLITE_LOCK_EXCLUSIVE),
      END,

      DICT("mapping_access"),
      ADDINT(SQLITE_ACCESS_EXISTS),
      ADDINT(SQLITE_ACCESS_READWRITE),
      ADDINT(SQLITE_ACCESS_READ),
      END,

      DICT("mapping_device_characteristics"),
      ADDINT(SQLITE_IOCAP_ATOMIC),
      ADDINT(SQLITE_IOCAP_ATOMIC512),
      ADDINT(SQLITE_IOCAP_ATOMIC1K),
      ADDINT(SQLITE_IOCAP_ATOMIC2K),
      ADDINT(SQLITE_IOCAP_ATOMIC4K),
      ADDINT(SQLITE_IOCAP_ATOMIC8K),
      ADDINT(SQLITE_IOCAP_ATOMIC16K),
      ADDINT(SQLITE_IOCAP_ATOMIC32K),
      ADDINT(SQLITE_IOCAP_ATOMIC64K),
      ADDINT(SQLITE_IOCAP_SAFE_APPEND),
      ADDINT(SQLITE_IOCAP_SEQUENTIAL),
      ADDINT(SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN),
      END,

      DICT("mapping_sync"),
      ADDINT(SQLITE_SYNC_NORMAL),
      ADDINT(SQLITE_SYNC_FULL),
      ADDINT(SQLITE_SYNC_DATAONLY),
      END,

      DICT("mapping_file_control"),
      ADDINT(SQLITE_FCNTL_LOCKSTATE),
      ADDINT(SQLITE_GET_LOCKPROXYFILE),
      ADDINT(SQLITE_SET_LOCKPROXYFILE),
      ADDINT(SQLITE_LAST_ERRNO),
      ADDINT(SQLITE_FCNTL_SIZE_HINT),
      ADDINT(SQLITE_FCNTL_CHUNK_SIZE),
      END

#ifdef APSW_USE_SQLITE_ASYNCVFS_H
      ,
      DICT("mapping_asyncvfs_control"),
      ADDINT(SQLITEASYNC_HALT),
      ADDINT(SQLITEASYNC_GET_HALT),
      ADDINT(SQLITEASYNC_DELAY),
      ADDINT(SQLITEASYNC_GET_DELAY),
      ADDINT(SQLITEASYNC_LOCKFILES),
      ADDINT(SQLITEASYNC_GET_LOCKFILES),
      END,

      DICT("mapping_asyncvfs_control_halt"),
      ADDINT(SQLITEASYNC_HALT_NEVER),
      ADDINT(SQLITEASYNC_HALT_NOW),
      ADDINT(SQLITEASYNC_HALT_IDLE),
      END
#endif
      };
 
 
 for(i=0;i<sizeof(integers)/sizeof(integers[0]); i++)
   {
     const char *name=integers[i].name;
     int value=integers[i].value;
     PyObject *pyname;
     PyObject *pyvalue;

     /* should be at dict */
     if(!thedict)
       {
         assert(value==SENTINEL);
         assert(mapping_name==NULL);
         mapping_name=name;
         thedict=PyDict_New();
         continue;
       }
     /* at END? */
     if(!name)
       {
         assert(thedict);
         PyModule_AddObject(m, mapping_name, thedict);
         thedict=NULL;
         mapping_name=NULL;
         continue;
       }
     /* regular ADDINT */
     PyModule_AddIntConstant(m, name, value);
     pyname=MAKESTR(name);
     pyvalue=PyInt_FromLong(value);
     if(!pyname || !pyvalue) goto fail;
     PyDict_SetItem(thedict, pyname, pyvalue);
     PyDict_SetItem(thedict, pyvalue, pyname);
     Py_DECREF(pyname);
     Py_DECREF(pyvalue);
   }
 /* should have ended with END so thedict should be NULL */
 assert(thedict==NULL);

 }

 add_shell(m);

 PyModule_AddObject(m, "compile_options", get_compile_options());

 if(!PyErr_Occurred())
      {
        return
#if PY_MAJOR_VERSION >= 3
          m
#endif
          ;
      }

 fail:
    Py_XDECREF(m);
    return 
#if PY_MAJOR_VERSION >= 3
          NULL
#endif
          ;
}

static void
add_shell(PyObject *apswmodule)
{
  PyObject *res=NULL, *maindict=NULL, *apswdict, *msvciscrap=NULL;

  maindict=PyModule_GetDict(PyImport_AddModule("__main__"));
  apswdict=PyModule_GetDict(apswmodule);
  PyDict_SetItemString(apswdict, "__builtins__", PyDict_GetItemString(maindict, "__builtins__"));
  PyDict_SetItemString(apswdict, "apsw", apswmodule);

  /* the toy compiler from microsoft falls over on string constants
     bigger than will fit in a 16 bit quantity.  You remember 16 bits?
     All the rage in the early 1980s.  So we have to compose chunks
     into a bytes and use that instead.  The format string is as many
     %s as there are chunks.  It is generated in setup.py.
  */
  msvciscrap=PyBytes_FromFormat(
#include "shell.c"
				);
  if(msvciscrap)
    res=PyRun_StringFlags(PyBytes_AS_STRING(msvciscrap),Py_file_input, apswdict, apswdict, NULL);
  if(!res) PyErr_Print();
  assert(res);
  Py_XDECREF(res);
  Py_XDECREF(msvciscrap);
}

#ifdef APSW_TESTFIXTURES
static int
APSW_Should_Fault(const char *name)
{
  PyGILState_STATE gilstate;
  PyObject *faultdict=NULL, *truthval=NULL, *value=NULL;
  int res=0;

  gilstate=PyGILState_Ensure();

  if(!PyObject_HasAttrString(apswmodule, "faultdict"))
    PyObject_SetAttrString(apswmodule, "faultdict", PyDict_New());

  value=MAKESTR(name);
  
  faultdict=PyObject_GetAttrString(apswmodule, "faultdict");
  
  truthval=PyDict_GetItem(faultdict, value);
  if(!truthval)
    goto finally;

  /* set false if present - one shot firing */
  PyDict_SetItem(faultdict, value, Py_False);
  res=PyObject_IsTrue(truthval);

 finally:
  Py_XDECREF(value);
  Py_XDECREF(faultdict);

  PyGILState_Release(gilstate);
  return res;
}
#endif

/* async vfs */
#ifdef APSW_USE_SQLITE_ASYNCVFS_C
#define SQLITE_ENABLE_ASYNCIO
#include APSW_USE_SQLITE_ASYNCVFS_C
#endif
