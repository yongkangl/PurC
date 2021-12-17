#!/usr/bin/python3

#
# Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
#
# This file is a part of Purring Cat 2, a HVML parser and interpreter.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Author: XueShuming

import os
import sys
import re

_license_header = """/*
 * Author: XueShuming
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

 // NOTE
 // This file is auto-generated by using 'make-error-info.py'.
 // Please take care when you modify this file mannually.

"""

def parse(file):
    output_file = ""
    output_var = ""
    messages = []
    for line in file:
        line = line.strip()
        match = re.search(r'OUTPUT_FILE=(.*)\s*', line)
        if match:
            output_file = match.groups()
            continue

        match = re.search(r'OUTPUT_VAR=(.*)\s*', line)
        if match:
            output_var = match.groups()
            continue

        match = re.search(r'OUTPUT=(.*)\s*', line)
        if match:
            filename = match.groups()

        match = re.search(r'([A-Za-z_0-9]+)\s+([A-Za-z_0-9]+)\s+([A-Za-z_0-9]+)\s+(.*)\s*', line)
        if match:
            error, except_name, flag, msg = match.groups()
            messages.append((error, except_name, flag, msg))
    return output_file, output_var, messages

def generate_inc(var_name, messages):
    result = []
    result.append(_license_header)
    result.append('\n')
    result.append('static struct err_msg_info %s[] = {\n' % var_name)
    for message in messages:
        result.append('    /* %s */\n' % message[0])
        result.append('    {\n')
        result.append('        %s,\n' % message[3])
        result.append('        %s,\n' % message[1])
        result.append('        %s,\n' % message[2])
        result.append('        0\n')
        result.append('    },\n')
    result.append('};\n')
    return ''.join(result)

if __name__ == "__main__":
    src_dir = None
    src_filename= None
    output_filename = None
    output_var_name = None
    messages = None
    if len(sys.argv) < 2:
        raise Exception('expecting *.error.in')

    first_arg = True
    for parameter in sys.argv:
        if first_arg:
            first_arg = False
            continue
        src_dir = os.path.dirname(parameter)
        src_filename = os.path.basename(parameter)

        if not src_dir:
            src_dir="."

        with open('%s/%s' % (src_dir, src_filename)) as source_file:
            output_filename, output_var_name, messages  = parse(source_file)

        with open('%s/%s' % (src_dir, ''.join(output_filename)), "w+") as output_file:
            output_file.write(generate_inc(output_var_name, messages))