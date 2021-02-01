#!/usr/bin/python3

"""wat2wasm4tests

This script converts C++ or Rust comments containing WebAssembly text format (WAT)
into WebAssembly binary format.

It uses wat2wasm tool from WABT (https://github.com/WebAssembly/wabt)
and expect the tool be available in PATH.
- On Linux/Debian wabt package is available.
- On macOS wabt can be installed via homebrew.

It searches for the C++ or Rust block comments starting with `wat2wasm [options]`.
Optional options are going to be passed to the wat2wasm tool.
Example:

    /* wat2wasm --no-check
    (module
      (func ...)
    )
    */
"""

import os
import re
import subprocess
import sys

DEBUG = False

WAT2WASM_TOOL = 'wat2wasm'
WAT2WASM_DEFAULT_OPTIONS = ['--disable-saturating-float-to-int',
                            '--disable-sign-extension', '--disable-multi-value']
CPP_FORMAT_TOOL = ['clang-format', '-i']
RS_FORMAT_TOOL = ['rustfmt']

CPP_EXTENSION = ".cpp"
RS_EXTENSION = ".rs"

WAT_RE = re.compile(r'/\* wat2wasm(.*)\n([^*]*)\*/', re.MULTILINE)
WASM_CPP_RE = re.compile(r'\s*(?:const )?auto \w+ =\s*(?:fizzy\:\:test\:\:)?from_hex\(\s*"([^;]*)"\);',
                     re.MULTILINE)
WASM_RUST_RE = re.compile(r'\s*let \w+ =\s*hex::decode\(\s*"([^;]*)"\).unwrap\(\);',
                     re.MULTILINE)

TMP_WAT_FILE = sys.argv[0] + '.wat'
TMP_WASM_FILE = sys.argv[0] + '.wasm'


def debug(*args, **kwargs):
    if DEBUG:
        print(*args, **kwargs)


def report_wat_errors(errmsg, source, source_path, wat_pos):
    wat_line = source.count('\n', 0, wat_pos)
    line_re = re.compile(TMP_WAT_FILE.replace('.', '\\.') + r':(\d+):')

    p = 0
    while True:
        line_match = line_re.search(errmsg, p)
        if not line_match:
            break
        n = int(line_match.group(1))
        s = line_match.span(1)
        errmsg = "".join((errmsg[:s[0]], str(wat_line + n), errmsg[s[1]:]))
        p = line_match.span()[1]

    errmsg = errmsg.replace(TMP_WAT_FILE, source_path)
    print(errmsg, file=sys.stderr)


try:
    if len(sys.argv) < 2:
        raise Exception("Missing FILE argument")

    source_path = sys.argv[1]

    source_extension = os.path.splitext(source_path)[1]
    if source_extension == CPP_EXTENSION:
        cpp_source = True
    elif source_extension == RS_EXTENSION:
        cpp_source = False
    else:
        raise Exception("File extension not supported")

    with open(source_path, 'r') as f:
        source = f.read()

    modified = False
    pos = 0

    while True:
        wat_match = WAT_RE.search(source, pos)
        if not wat_match:
            break

        wat = wat_match.group(2)
        options = wat_match.group(1)
        options = options.split() if options else []
        pos = wat_match.span()[1]

        debug(f"wat2wasm: {options}\n{wat}")

        with open(TMP_WAT_FILE, 'w') as f:
            f.write(wat)

        options = WAT2WASM_DEFAULT_OPTIONS + options
        r = subprocess.run([WAT2WASM_TOOL, TMP_WAT_FILE] + options,
                           capture_output=True, text=True)
        if r.returncode != 0:
            report_wat_errors(r.stderr, source, source_path,
                              wat_match.span(2)[0])
            continue

        with open(TMP_WASM_FILE, 'rb') as f:
            new_wasm = f.read().hex()

        wasm_re = WASM_CPP_RE if cpp_source else WASM_RUST_RE
        wasm_match = wasm_re.match(source, pos)
        if wasm_match:
            cur_wasm = wasm_match.group(1)
            cur_wasm = cur_wasm.translate({ord(c): '' for c in ' \r\n"'})
            if new_wasm != cur_wasm:
                modified = True
                begin, end = wasm_match.span(1)
                source = "".join((source[:begin], new_wasm, source[end:]))
        else:
            modified = True
            if cpp_source:
                source = "".join((source[:pos], 'const auto wasm = from_hex("',
                                  new_wasm, '");', source[pos:]))
            else:
                source = "".join((source[:pos], 'let wasm = hex::decode("',
                                  new_wasm, '").unwrap();', source[pos:]))


    if modified:
        with open(source_path, 'w') as f:
            f.write(source)

        # Format the modified file with clang-format / rustfmt, but ignore all the related
        # errors, including formatting tool not found.
        format_tool = CPP_FORMAT_TOOL if cpp_source else RS_FORMAT_TOOL
        try:
            subprocess.run(format_tool + [source_path],
                           capture_output=True)
        except:
            pass


except Exception as ex:
    print(f"{ex}", file=sys.stderr)

if os.path.exists(TMP_WAT_FILE):
    os.remove(TMP_WAT_FILE)
if os.path.exists(TMP_WASM_FILE):
    os.remove(TMP_WASM_FILE)
