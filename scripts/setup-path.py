#!/usr/bin/env python2
"""\
@file setup-path.py
@brief Get the python library directory in the path, so we don't have
to screw with PYTHONPATH or symbolic links.

$LicenseInfo:firstyear=2007&license=viewergpl$

Copyright (c) 2007-2009, Linden Research, Inc.

Second Life Viewer Source Code
The source code in this file ("Source Code") is provided by Linden Lab
to you under the terms of the GNU General Public License, version 2.0
("GPL"), unless you have obtained a separate licensing agreement
("Other License"), formally executed by you and Linden Lab.  Terms of
the GPL can be found in doc/GPL-license.txt in this distribution, or
online at http://secondlifegrid.net/programs/open_source/licensing/gplv2

There are special exceptions to the terms and conditions of the GPL as
it is applied to this Source Code. View the full text of the exception
in the file doc/FLOSS-exception.txt in this software distribution, or
online at
http://secondlifegrid.net/programs/open_source/licensing/flossexception

By copying, modifying or distributing this software, you acknowledge
that you have read and understood your obligations described above,
and agree to abide by those obligations.

ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
COMPLETENESS OR PERFORMANCE.
$/LicenseInfo$
"""

import sys
from os.path import realpath, dirname, join

# Walk back to checkout base directory
dir = dirname(dirname(realpath(__file__)))
# Walk in to libraries directory
dir = join(dir, 'indra', 'lib', 'python')

if dir not in sys.path:
    sys.path.insert(0, dir)
