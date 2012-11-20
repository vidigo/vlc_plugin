/*****************************************************************************
 * vout.c: Dummy video output display method for testing purposes
 *
 * Modified by VidiGo to create a bridge between VLC and our own code base
 *
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id: 3caf795fe7d22eb9c1daf0d4cd61104ec6bdefdb $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_aout.h>
#include <vlc_aout_mixer.h>

#include <windows.h>

#define HAVE_LLDIV
#define HAVE_STRDUP
#define HAVE_GMTIME_R
#define HAVE_LOCALTIME_R
#define HAVE_GETENV
#include <vlc_fixups.h>

static void Close( vlc_object_t * );

struct vout_display_sys_t {
	picture_pool_t *pool;
};

typedef void (*AudioCallback)( audio_output_t*, block_t* );
typedef void (*VideoCallback)( vout_display_t*, picture_t*, subpicture_t* );

static picture_pool_t *Pool(vout_display_t *, unsigned count);
static void	Video(vout_display_t *, picture_t *, subpicture_t *);
static void Audio( audio_output_t *audio, block_t *block );
static void Flush( audio_output_t *audio );
static void Pause(audio_output_t * audio, bool pause, mtime_t date );
static int	Control(vout_display_t *, int, va_list);

static int openAudio( vlc_object_t *object );
static int openVideo( vlc_object_t *object );

typedef void (*VidigoGetFrame)( void *YUVY,
								uint32_t width, uint32_t height,
								uint32_t pitch, uint32_t visiblePitch,
								uint32_t lines, uint32_t visibleLines,
								int32_t frame_rate, int32_t frame_rate_base,
								bool top, bool prog, mtime_t pts );

typedef void (*VidigoGetAudio)( void *audio, uint32_t count, uint32_t channels, mtime_t pts );
typedef void (*VidigoFlush)();
typedef void (*VidigoPause)( bool paused );

VidigoGetFrame vidigoGetFrame = 0;
VidigoGetAudio vidigoGetAudio = 0;
VidigoFlush vidigoFlush = 0;
VidigoPause vidigoPause = 0;

static int dummyClose()
{
	return VLC_SUCCESS;
}

vlc_module_begin ()
	set_shortname( N_("Vidigo") )
	set_description( N_("Vidigo output") )
	add_shortcut( "vidigo" )

	add_submodule ()
	set_shortname( N_("Vidigo") )
	set_description( N_("Vidigo output") )
	set_category( CAT_VIDEO )
	set_subcategory( SUBCAT_VIDEO_VOUT )
	set_capability( "vout display", 0 )
	set_callbacks( openVideo, Close )

	add_submodule ()
	set_shortname( N_("Vidigo") )
	set_description( N_("Vidigo output") )
	set_category( CAT_AUDIO )
	set_subcategory( SUBCAT_AUDIO_AOUT )
	set_capability( "audio output", 0 )
	set_callbacks( openAudio, dummyClose )

vlc_module_end ()

static int openAudio( vlc_object_t *object )
{
	HMODULE module = GetModuleHandle( "davevlc.dll" );

	if( !module )
	{
		module = GetModuleHandle( "davevlcd.dll" );
	}

	if ( module )
	{
		vidigoGetAudio = GetProcAddress( module, "VidigoGetAudio" );

		vidigoFlush = GetProcAddress( module, "VidigoFlush" );

		vidigoPause = GetProcAddress( module, "VidigoPause" );
	}

	audio_output_t * p_aout = (audio_output_t *)object;

	p_aout->pf_play = Audio;
	p_aout->pf_pause = Pause;
	p_aout->pf_flush = Flush;
	aout_VolumeSoftInit( p_aout );

	p_aout->format.i_format = VLC_CODEC_FL32;
	p_aout->format.i_rate = 48000;

	return VLC_SUCCESS;
}

static int openVideo( vlc_object_t *object )
{
	HMODULE module = GetModuleHandle( "davevlc.dll" );

	if( !module )
	{
		module = GetModuleHandle( "davevlcd.dll" );
	}

	if ( module )
	{
		vidigoGetFrame = GetProcAddress( module, "VidigoGetFrame" );
	}

	// VIDEO

	vout_display_t *vd = (vout_display_t *)object;
	vout_display_sys_t *sys;

	vd->sys = sys = calloc(1, sizeof(*sys));
	if (!sys)
		return VLC_EGENERIC;
	sys->pool = NULL;

	/* p_vd->info is not modified */

	vlc_fourcc_t fcc = vlc_fourcc_GetCodecFromString(VIDEO_ES, "UYVY");
	if ( fcc != 0 )
	{
		msg_Dbg(vd, "forcing chroma 0x%.8x (%4.4s)", fcc, (char*)&fcc);
		vd->fmt.i_chroma = fcc;
	}

	vd->pool    = Pool;
	vd->prepare = NULL;
	vd->display = Video;
	vd->control = Control;
	vd->manage  = NULL;

	return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
	vout_display_t *vd = (vout_display_t *)object;
	vout_display_sys_t *sys = vd->sys;

	if (sys->pool)
		picture_pool_Delete(sys->pool);
	free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
	vout_display_sys_t *sys = vd->sys;

	if (!sys->pool)
	{
		sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
	}

	return sys->pool;
}

static void Video(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
	if ( vidigoGetFrame )
	{
		vidigoGetFrame(
				picture->p[ 0 ].p_pixels,
				picture->format.i_width, picture->format.i_height,
				picture->p[ 0 ].i_pitch, picture->p[ 0 ].i_visible_pitch,
				picture->p[ 0 ].i_lines, picture->p[ 0 ].i_visible_lines,
				vd->fmt.i_frame_rate, vd->fmt.i_frame_rate_base,
				picture->b_top_field_first,
				picture->b_progressive,
				picture->date );
	}

	VLC_UNUSED(vd);
	VLC_UNUSED(subpicture);
	picture_Release(picture);
}

static void Audio( audio_output_t *audio, block_t *block )
{
	if ( vidigoGetAudio )
	{
		aout_packet_t *packet = audio;

		vidigoGetAudio( block->p_buffer, block->i_nb_samples, audio->format.i_channels, block->i_pts );
	}

	VLC_UNUSED( audio );
	block_Release( block );
}

static void Flush( audio_output_t *audio )
{
	if ( vidigoFlush )
	{
		vidigoFlush();
	}

	VLC_UNUSED( audio );
}

static void Pause( audio_output_t *audio, bool pause, mtime_t date )
{
	if ( vidigoPause )
	{
		vidigoPause( pause );
	}

	VLC_UNUSED( audio );
}

static int Control(vout_display_t *vd, int query, va_list args)
{
	VLC_UNUSED(vd);
	VLC_UNUSED(query);
	VLC_UNUSED(args);
	return VLC_SUCCESS;
}
