/* $Id: ss_interp.h,v 1.3 2001/03/29 16:50:33 brianp Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keithw@valinux.com>
 */

#ifndef SS_INTERP_H
#define SS_INTERP_H

#include "mtypes.h"
#include "swrast_setup.h"

void _swsetup_interp_init( GLcontext *ctx );

#define _SWSETUP_NEW_INTERP   (_NEW_BUFFERS |			\
			       _DD_NEW_SEPARATE_SPECULAR |	\
			       _NEW_TEXTURE|			\
			       _NEW_FOG|			\
			       _DD_NEW_TRI_UNFILLED |		\
			       _NEW_RENDERMODE |		\
			       _DD_NEW_FLATSHADE)

void _swsetup_validate_interp( GLcontext *ctx, GLfloat t,
			       GLuint dst, GLuint out, GLuint in,
			       GLboolean force_boundary );

void _swsetup_validate_copypv( GLcontext *ctx, GLuint dst, GLuint src );


#endif