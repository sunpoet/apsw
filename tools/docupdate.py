# python
#
# See the accompanying LICENSE file.
#
# various automagic documentation updates

import sys

# get the download file names correct

version = sys.argv[1]
url = "  <https://github.com/rogerbinns/apsw/releases/download/" + version + "/%s>`__"

pip_template = """
.. pip-begin

Use this (all one command)::

    pip install --user https://github.com/rogerbinns/apsw/releases/download/%s/apsw-%s.zip \\
    --global-option=fetch --global-option=--version --global-option=%s --global-option=--all \\
    --global-option=build --global-option=--enable-all-extensions
""" % (version, version, version.split("-")[0])

download = open("doc/download.rst", "rt").read()

op = []
incomment = False
for line in open("doc/download.rst", "rt"):
    line = line.rstrip()
    if line == ".. downloads-begin":
        op.append(line)
        incomment = True
        op.append("")
        op.append("* `apsw-%s.zip" % (version, ))
        op.append(url % ("apsw-%s.zip" % version))
        op.append("  (Source, includes this HTML Help)")
        op.append("")
        not64 = ("2.3", "2.4", "2.5")
        not32 = ("3.5", )
        for pyver in ("2.3", "2.4", "2.5", "2.6", "2.7", "3.1", "3.2", "3.3", "3.4", "3.5", "3.6", "3.7", "3.8"):
            op.append("* Windows Python %s" % (pyver, ))
            if pyver not in not32:
                op.append("  `32bit ")
                op.append(url % ("apsw-%s.win32-py%s.exe" % (version, pyver)))
            if pyver not in not64:
                op.append("  `64bit ")
                op.append(url % ("apsw-%s.win-amd64-py%s.exe" % (version, pyver)))
            op.append("")
        op.append("* `apsw-%s-sigs.zip " % (version, ))
        op.append(url % ("apsw-%s-sigs.zip" % version))
        op.append("  GPG signatures for all files")
        op.append("")
        continue
    if line == ".. pip-begin":
        op.extend(pip_template.split("\n")[1:])
        incomment = True
        continue
    if line == ".. downloads-end" or line == ".. pip-end":
        incomment = False
    if incomment:
        continue
    if line.lstrip().startswith("$ gpg --verify apsw"):
        line = line[:line.index("$")] + "$ gpg --verify apsw-%s.zip.asc" % (version, )
    op.append(line)

op = "\n".join(op)
if op != download:
    open("doc/download.rst", "wt").write(op)

# put usage and description for speedtest into benchmark

import speedtest

benchmark = open("doc/benchmarking.rst", "rt").read()

op = []
incomment = False
for line in open("doc/benchmarking.rst", "rt"):
    line = line.rstrip()
    if line == ".. speedtest-begin":
        op.append(line)
        incomment = True
        op.append("")
        op.append(".. code-block:: text")
        op.append("")
        op.append("    $ python speedtest.py --help")
        speedtest.parser.set_usage("Usage: speedtest.py [options]")
        for line in speedtest.parser.format_help().split("\n"):
            op.append("    " + line)
        op.append("")
        op.append("    $ python speedtest.py --tests-detail")
        for line in speedtest.tests_detail.split("\n"):
            op.append("    " + line)
        op.append("")
        continue
    if line == ".. speedtest-end":
        incomment = False
    if incomment:
        continue
    op.append(line)

op = "\n".join(op)
if op != benchmark:
    open("doc/benchmarking.rst", "wt").write(op)

# shell stuff

import apsw, io
shell = apsw.Shell()
incomment = False
op = []
for line in open("doc/shell.rst", "rt"):
    line = line.rstrip()
    if line == ".. help-begin:":
        op.append(line)
        incomment = True
        op.append("")
        op.append(".. code-block:: text")
        op.append("")
        s = io.StringIO()

        def tw(*args):
            return 80

        shell.stderr = s
        shell._terminal_width = tw
        shell.command_help([])
        op.extend(["  " + x for x in s.getvalue().split("\n")])
        op.append("")
        continue
    if line == ".. usage-begin:":
        op.append(line)
        incomment = True
        op.append("")
        op.append(".. code-block:: text")
        op.append("")
        op.extend(["  " + x for x in shell.usage().split("\n")])
        op.append("")
        continue
    if line == ".. help-end:":
        incomment = False
    if line == ".. usage-end:":
        incomment = False
    if incomment:
        continue
    op.append(line)

op = "\n".join(op)
if op != open("doc/shell.rst", "rt").read():
    open("doc/shell.rst", "wt").write(op)
