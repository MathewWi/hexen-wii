/*   
	Copyright (C) 2010 Arikado
	
	Some sections Copyright (C) 2010 Hermes
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gccore.h>
#include <string.h>
#include <malloc.h>

#include <stdlib.h>
#include <unistd.h>

#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>

#include "screen.h"
#include <asndlib.h>

#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

#include "h2def.h"
#include "r_local.h"
#include "p_local.h"    // for P_AproxDistance
#include "sounds.h"
#include "i_sound.h"
#include "soundst.h"
#include "st_start.h"

// Macros

#define stricmp strcasecmp
#define DEFAULT_ARCHIVEPATH     ""
#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)


char *str_trace= "";

int16_t ShortSwap(int16_t b)
{

	b=(b<<8) | ((b>>8) & 0xff);

	return b;
}

int32_t LongSwap(int32_t b)
{

	b=(b<<24) | ((b>>24) & 0xff) | ((b<<8) & 0xff0000) | ((b>>8) & 0xff00);

	return b;
}

int is_16_9=0;

//byte *pcscreen, *destscreen;

#include <wiiuse/wpad.h>

#define TIME_SLEEP_SCR 5*60

static syswd_t scr_poweroff;

int time_sleep=0;


static void scr_poweroff_handler(syswd_t alarm, void * arg)
{	
	if(time_sleep>1) time_sleep--;
	if(time_sleep==1) SetVideoSleep(1);
	
}

unsigned temp_pad=0,new_pad=0,old_pad=0;

WPADData * wmote_datas=NULL;


unsigned wiimote_read()
{
int n;

int ret=-1;

unsigned type=0;

unsigned butt=0;

	wmote_datas=NULL;

	//w_index=-1;

	for(n=0;n<4;n++) // busca el primer wiimote encendido y usa ese
		{
		ret=WPAD_Probe(n, &type);

		if(ret>=0)
			{
			
			butt=WPAD_ButtonsHeld(n);
			
				wmote_datas=WPAD_Data(n);

		//	w_index=n;

			break;
			}
		}

	if(n==4) butt=0;

		temp_pad=butt;

		new_pad=temp_pad & (~old_pad);old_pad=temp_pad;

	if(new_pad)
		{
		time_sleep=TIME_SLEEP_SCR;
		SetVideoSleep(0);
		}

return butt;
}


int exit_by_reset=0;

int return_reset=2;

void reset_call() {exit_by_reset=return_reset;}
void power_call() {exit_by_reset=3;}


// Public Data

int DisplayTicker = 0;

// sd mounted?

int sd_ok=0;

extern Mtx	modelView; // screenlib matrix (used for 16:9 screens)

u32 *wii_scr, *wii_scr2; // wii texture displayed

GXTexObj text_scr, text_scr2;


#define TEXT_W SCREENWIDTH
#define TEXT_H SCREENHEIGHT

void wii_test(u32 color)
{
int n;
	
	for(n=0;n<TEXT_W * TEXT_H; n++) wii_scr[n]=color;

	CreateTexture(&text_scr, TILE_RGBA8 , wii_scr, TEXT_W , TEXT_H, 0);
	
	SetTexture(&text_scr);

	DrawFillBox(0, -12, SCR_WIDTH, SCR_HEIGHT+24, 999, 0xffffffff);

	SetTexture(NULL);

	Screen_flip();
}

void My_Quit(void)
{
	WPAD_Shutdown();
	ASND_End();

	SYS_RemoveAlarm(scr_poweroff);

	if(sd_ok)
		{
		fatUnmount("sd");
		__io_wiisd.shutdown();sd_ok=0;
		}

	sleep(1);

	if(exit_by_reset==2)
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	if(exit_by_reset==3)
		SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);
	return;
}

int main(int argc, char **argv)
{
	struct timespec tb;
	myargc = 0;//argc;
	myargv = NULL;//argv;

	if (*((u32*)0x80001800) && strncmp("STUBHAXX", (char *)0x80001804, 8) == 0) return_reset=1;
	else return_reset=2;

	if(CONF_Init()==0)
	{
		is_16_9=CONF_GetAspectRatio()!=0;
		
	}

	SYS_SetResetCallback(reset_call); // esto es para que puedas salir al pulsar boton de RESET
	SYS_SetPowerCallback(power_call); // esto para apagar con power

	InitScreen();  // Inicialización del Video

	//pcscreen = destscreen = memalign(32, SCREENWIDTH * SCREENHEIGHT);
	wii_scr= memalign(32, TEXT_W * (TEXT_H+8) *4); // for game texture
	wii_scr2= memalign(32, TEXT_W * (TEXT_H+8) *4); // for menu texture

	wii_test(0x0);

	if(is_16_9)
		{
		ChangeProjection(0,SCR_HEIGHT<=480 ? -12: 0,848,SCR_HEIGHT+(SCR_HEIGHT<=480 ? 16: 0));
		guMtxIdentity(modelView);

		GX_SetCurrentMtx(GX_PNMTX0); // selecciona la matriz
		guMtxTrans(modelView, 104.0f, 0.0f, 0.0f);
		GX_LoadPosMtxImm(modelView,	GX_PNMTX0); // carga la matriz mundial como identidad
		
		}
	
	char title1[]="Hexen for Wii";
	char title2[]="Ported by Arikado";
	char title3[]="http://arikadosblog.blogspot.com";

	//char title1[]=

	letter_size(16, 32);

	PY=32;PX=(640-strlen(title1)*16)/2;
	s_printf ("%s", title1);

	PY=32+64;PX=(640-strlen(title2)*16)/2;
	s_printf("%s", title2);

	PY=32+192;PX=(640-strlen(title3)*16)/2;
	s_printf("%s", title3);

	Screen_flip();

	letter_size(12, 24);

    PAD_Init();
	WPAD_Init();
	WPAD_SetIdleTimeout(60*5); // 5 minutes 

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	
	if(__io_wiisd.startup())
		{
		sd_ok = fatMountSimple("sd", &__io_wiisd);
		if(!sd_ok) 
			{	__io_wiisd.shutdown();
				WPAD_Shutdown();
				sleep(1); 

				if(exit_by_reset==2)
					SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
				if(exit_by_reset==3)
					SYS_ResetSystem(SYS_POWEROFF_STANDBY, 0, 0);

				return 0;
			}
		}

	ASND_Init();
	ASND_Pause(0);
	
	SYS_CreateAlarm(&scr_poweroff);
	tb.tv_sec = 1;tb.tv_nsec = 0;
	SYS_SetPeriodicAlarm(scr_poweroff, &tb, &tb, scr_poweroff_handler, NULL);
	atexit (My_Quit);

	H2_Main();

    return 0;
}

void I_StartupNet (void);
void I_ShutdownNet (void);
void I_ReadExternDriver(void);


extern int usemouse, usejoystick;

extern void **lumpcache;

BOOLEAN i_CDMusic;
int i_CDTrack;
int i_CDCurrentTrack;
int i_CDMusicLength;
int oldTic;

int grabMouse;

/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/


extern sfxinfo_t S_sfx[];
extern musicinfo_t S_music[];

static channel_t Channel[MAX_CHANNELS];
static int RegisteredSong; //the current registered song.
static int NextCleanup;
static BOOLEAN MusicPaused;
static int Mus_Song = -1;
static int Mus_LumpNum;
static void *Mus_SndPtr;
static byte *SoundCurve;

static BOOLEAN UseSndScript;
static char ArchivePath[128];

extern int snd_MusicDevice;
extern int snd_SfxDevice;
extern int snd_MaxVolume;
extern int snd_MusicVolume;
extern int snd_Channels;

extern int startepisode;
extern int startmap;

// int AmbChan;

BOOLEAN S_StopSoundID(int sound_id, int priority);
void I_UpdateCDMusic(void);

//==========================================================================
//
// S_Start
//
//==========================================================================

void S_Start(void)
{
	S_StopAllSound();
	S_StartSong(gamemap, true);
}

//==========================================================================
//
// S_StartSong
//
//==========================================================================

void S_StartSong(int song, BOOLEAN loop)
{
	char *songLump;
	int track;

	if(i_CDMusic)
	{ // Play a CD track, instead
		
		if(i_CDTrack)
		{ // Default to the player-chosen track
			track = i_CDTrack;
		}
		else
		{
			track = P_GetMapCDTrack(gamemap);
		}
		if(track == i_CDCurrentTrack && i_CDMusicLength > 0)
		{
			return;
		}
		#if 0
		if(!I_CDMusPlay(track))
		{
			if(loop)
			{
				i_CDMusicLength = 35*I_CDMusTrackLength(track);
				oldTic = gametic;
			}
			else
			{
				i_CDMusicLength = -1;
			}
			i_CDCurrentTrack = track;
		}
		#endif
		i_CDMusicLength = -1;
	}
	else
	{
		if(song == Mus_Song)
		{ // don't replay an old song
			return;
		}
		if(RegisteredSong)
		{
			I_StopSong(RegisteredSong);
			I_UnRegisterSong(RegisteredSong);
			if(UseSndScript)
			{
				Z_Free(Mus_SndPtr);
			}
			else
			{
				Z_ChangeTag(lumpcache[Mus_LumpNum], PU_CACHE);
			}
			#ifdef __WATCOMC__
				_dpmi_unlockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
			#endif
			RegisteredSong = 0;
		}
		songLump = P_GetMapSongLump(song);
		if(!songLump)
		{
			return;
		}
		if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, songLump);
			M_ReadFile(name, (void *)&Mus_SndPtr);
		}
		else
		{
			Mus_LumpNum = W_GetNumForName(songLump);
			Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		}
		#ifdef __WATCOMC__
			_dpmi_lockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
		#endif
		RegisteredSong = I_RegisterSong(Mus_SndPtr);
		I_PlaySong(RegisteredSong, loop); // 'true' denotes endless looping.
		Mus_Song = song;
	}
}

//==========================================================================
//
// S_StartSongName
//
//==========================================================================

void S_StartSongName(char *songLump, BOOLEAN loop)
{
	int cdTrack;

	if(!songLump)
	{
		return;
	}
	if(i_CDMusic)
	{
		cdTrack = 0;

		if(!strcmp(songLump, "hexen"))
		{
			cdTrack = P_GetCDTitleTrack();
		}
		else if(!strcmp(songLump, "hub"))
		{
			cdTrack = P_GetCDIntermissionTrack();
		}
		else if(!strcmp(songLump, "hall"))
		{
			cdTrack = P_GetCDEnd1Track();
		}
		else if(!strcmp(songLump, "orb"))
		{
			cdTrack = P_GetCDEnd2Track();
		}
		else if(!strcmp(songLump, "chess") && !i_CDTrack)
		{
			cdTrack = P_GetCDEnd3Track();
		}
/*	Uncomment this, if Kevin writes a specific song for startup
		else if(!strcmp(songLump, "start"))
		{
			cdTrack = P_GetCDStartTrack();
		}
*/
		if(!cdTrack || (cdTrack == i_CDCurrentTrack && i_CDMusicLength > 0))
		{
			return;
		}

		#if 0
		if(!I_CDMusPlay(cdTrack))
		{
			if(loop)
			{
				i_CDMusicLength = 35*I_CDMusTrackLength(cdTrack);
				oldTic = gametic;
			}
			else
			{
				i_CDMusicLength = -1;
			}
			i_CDCurrentTrack = cdTrack;
			i_CDTrack = false;
		}
		#endif
	}
	else
	{
		if(RegisteredSong)
		{
			I_StopSong(RegisteredSong);
			I_UnRegisterSong(RegisteredSong);
			if(UseSndScript)
			{
				Z_Free(Mus_SndPtr);
			}
			else
			{
				Z_ChangeTag(lumpcache[Mus_LumpNum], PU_CACHE);
			}
			#ifdef __WATCOMC__
				_dpmi_unlockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
			#endif
			RegisteredSong = 0;
		}
		if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, songLump);
			M_ReadFile(name, (void *)&Mus_SndPtr);
		}
		else
		{
			Mus_LumpNum = W_GetNumForName(songLump);
			Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		}
		#ifdef __WATCOMC__
			_dpmi_lockregion(Mus_SndPtr, lumpinfo[Mus_LumpNum].size);
		#endif
		RegisteredSong = I_RegisterSong(Mus_SndPtr);
		I_PlaySong(RegisteredSong, loop); // 'true' denotes endless looping.
		Mus_Song = -1;
	}
}

//==========================================================================
//
// S_GetSoundID
//
//==========================================================================

int S_GetSoundID(char *name)
{
	int i;

	for(i = 0; i < NUMSFX; i++)
	{
		if(!strcmp(S_sfx[i].tagName, name))
		{
			return i;
		}
	}
	return 0;
}

//==========================================================================
//
// S_StartSound
//
//==========================================================================

void S_StartSound(mobj_t *origin, int sound_id)
{
	S_StartSoundAtVolume(origin, sound_id, 127);
}

//==========================================================================
//
// S_StartSoundAtVolume
//
//==========================================================================

void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume)
{

	int dist, vol;
	int i;
	int priority;
	int sep;
	int angle;
	int absx;
	int absy;

	static int sndcount = 0;
	int chan;

    //printf( "S_StartSoundAtVolume: %d\n", sound_id );

	if(sound_id == 0 || snd_MaxVolume == 0)
		return;
	if(origin == NULL)
	{
		origin = players[displayplayer].mo;
        // How does the DOS version work without this?
        // players[0].mo does not get set until P_SpawnPlayer. - KR
        if( origin == NULL )
            return;

	}
	if(volume == 0)
	{
		return;
	}

	// calculate the distance before other stuff so that we can throw out
	// sounds that are beyond the hearing range.
	absx = abs(origin->x-players[displayplayer].mo->x);
	absy = abs(origin->y-players[displayplayer].mo->y);
	dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
	dist >>= FRACBITS;
	if(dist >= MAX_SND_DIST)
	{
	  return; // sound is beyond the hearing range...
	}
	if(dist < 0)
	{
		dist = 0;
	}
	priority = S_sfx[sound_id].priority;
	priority *= (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
	if(!S_StopSoundID(sound_id, priority))
	{
		return; // other sounds have greater priority
	}
	for(i=0; i<snd_Channels; i++)
	{
		if(origin->player)
		{
			i = snd_Channels;
			break; // let the player have more than one sound.
		}
		if(origin == Channel[i].mo)
		{ // only allow other mobjs one sound
			S_StopSound(Channel[i].mo);
			break;
		}
	}
	if(i >= snd_Channels)
	{
		for(i = 0; i < snd_Channels; i++)
		{
			if(Channel[i].mo == NULL)
			{
				break;
			}
		}
		if(i >= snd_Channels)
		{
			// look for a lower priority sound to replace.
			sndcount++;
			if(sndcount >= snd_Channels)
			{
				sndcount = 0;
			}
			for(chan = 0; chan < snd_Channels; chan++)
			{
				i = (sndcount+chan)%snd_Channels;
				if(priority >= Channel[i].priority)
				{
					chan = -1; //denote that sound should be replaced.
					break;
				}
			}
			if(chan != -1)
			{
				return; //no free channels.
			}
			else //replace the lower priority sound.
			{
				if(Channel[i].handle)
				{
					if(I_SoundIsPlaying(Channel[i].handle))
					{
						I_StopSound(Channel[i].handle);
					}
					if(S_sfx[Channel[i].sound_id].usefulness > 0)
					{
						S_sfx[Channel[i].sound_id].usefulness--;
					}
				}
			}
		}
	}
	if(S_sfx[sound_id].lumpnum == 0)
	{
		S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
	}
	if(S_sfx[sound_id].snd_ptr == NULL)
	{
		if(0/*UseSndScript*/)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, S_sfx[sound_id].lumpname);
			M_ReadFile(name, (void *)&S_sfx[sound_id].snd_ptr);
		}
		else
		{
		int flag_convert=0;

			if (!lumpcache[S_sfx[sound_id].lumpnum]) flag_convert=1;
			S_sfx[sound_id].snd_ptr = W_CacheLumpNum(S_sfx[sound_id].lumpnum,
				PU_SOUND);

		//memset(S_sfx[sound_id].snd_ptr,0, W_LumpLength(S_sfx[sound_id].lumpnum));
			if(flag_convert)
				{
				int n,len;
				u8 *p;

				p=S_sfx[sound_id].snd_ptr;
				
				len=W_LumpLength(S_sfx[sound_id].lumpnum);
				//for(n=0;n<len;n++) {(*p++)+=128;}
				for(n=0;n<len;n++) {(*p++)-=127;}

				}
		}
		#ifdef __WATCOMC__
		_dpmi_lockregion(S_sfx[sound_id].snd_ptr,
			lumpinfo[S_sfx[sound_id].lumpnum].size);
		#endif
	}

	vol = (SoundCurve[dist]*(snd_MaxVolume*8)*volume)>>14;
	if(origin == players[displayplayer].mo)
	{
		sep = 128;
//              vol = (volume*(snd_MaxVolume+1)*8)>>7;
	}
	else
	{

        // KR - Channel[i].mo = 0 here!
        if( Channel[i].mo == NULL )
        {
            sep = 128;
            //printf( " Channel[i].mo not set\n" );
        }
        else
        
		{
		angle = R_PointToAngle2(players[displayplayer].mo->x,
			players[displayplayer].mo->y, Channel[i].mo->x, Channel[i].mo->y);
		angle = (angle-viewangle)>>24;
		sep = angle*2-128;
		if(sep < 64)
			sep = -sep;
		if(sep > 192)
			sep = 512-sep;
//              vol = SoundCurve[dist];

        }

	}

	if(S_sfx[sound_id].changePitch)
	{
		Channel[i].pitch = (byte)(127+(M_Random()&7)-(M_Random()&7));
	}
	else
	{
		Channel[i].pitch = 127;
	}
	Channel[i].handle = I_StartSound(sound_id, S_sfx[sound_id].snd_ptr, vol,
		sep, Channel[i].pitch, 0);
	Channel[i].mo = origin;
	Channel[i].sound_id = sound_id;
	Channel[i].priority = priority;
	Channel[i].volume = volume;
	if(S_sfx[sound_id].usefulness < 0)
	{
		S_sfx[sound_id].usefulness = 1;
	}
	else
	{
		S_sfx[sound_id].usefulness++;
	}

}

//==========================================================================
//
// S_StopSoundID
//
//==========================================================================

BOOLEAN S_StopSoundID(int sound_id, int priority)
{
	int i;
	int lp; //least priority
	int found;

	if(S_sfx[sound_id].numchannels == -1)
	{
		return(true);
	}
	lp = -1; //denote the argument sound_id
	found = 0;
	for(i=0; i<snd_Channels; i++)
	{
		if(Channel[i].sound_id == sound_id && Channel[i].mo)
		{
			found++; //found one.  Now, should we replace it??
			if(priority >= Channel[i].priority)
			{ // if we're gonna kill one, then this'll be it
				lp = i;
				priority = Channel[i].priority;
			}
		}
	}
	if(found < S_sfx[sound_id].numchannels)
	{
		return(true);
	}
	else if(lp == -1)
	{
		return(false); // don't replace any sounds
	}
	if(Channel[lp].handle)
	{
		if(I_SoundIsPlaying(Channel[lp].handle))
		{
			I_StopSound(Channel[lp].handle);
		}
		if(S_sfx[Channel[lp].sound_id].usefulness > 0)
		{
			S_sfx[Channel[lp].sound_id].usefulness--;
		}
		Channel[lp].mo = NULL;
	}
	return(true);
}

//==========================================================================
//
// S_StopSound
//
//==========================================================================

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(Channel[i].mo == origin)
		{
			I_StopSound(Channel[i].handle);
			if(S_sfx[Channel[i].sound_id].usefulness > 0)
			{
				S_sfx[Channel[i].sound_id].usefulness--;
			}
			Channel[i].handle = 0;
			Channel[i].mo = NULL;
		}
	}
}

//==========================================================================
//
// S_StopAllSound
//
//==========================================================================

void S_StopAllSound(void)
{
	int i;

	//stop all sounds
	for(i=0; i < snd_Channels; i++)
	{
		if(Channel[i].handle)
		{
			S_StopSound(Channel[i].mo);
		}
	}
	memset(Channel, 0, 8*sizeof(channel_t));
}

//==========================================================================
//
// S_SoundLink
//
//==========================================================================

void S_SoundLink(mobj_t *oldactor, mobj_t *newactor)
{
	int i;

	for(i=0;i<snd_Channels;i++)
	{
		if(Channel[i].mo == oldactor)
			Channel[i].mo = newactor;
	}
}

//==========================================================================
//
// S_PauseSound
//
//==========================================================================

void S_PauseSound(void)
{
	if(i_CDMusic)
	{
		//I_CDMusStop();
	}
	else
	{
		I_PauseSong(RegisteredSong);
	}
}

//==========================================================================
//
// S_ResumeSound
//
//==========================================================================

void S_ResumeSound(void)
{
	if(i_CDMusic)
	{
		//I_CDMusResume();
	}
	else
	{
		I_ResumeSong(RegisteredSong);
	}
}

//==========================================================================
//
// S_UpdateSounds
//
//==========================================================================

void S_UpdateSounds(mobj_t *listener)
{
	int i, dist, vol;
	int angle;
	int sep;
	int priority;
	int absx;
	int absy;

	if(i_CDMusic)
	{
		//I_UpdateCDMusic();
	}
	if(snd_MaxVolume == 0)
	{
		return;
	}

	// Update any Sequences
	SN_UpdateActiveSequences();

	if(NextCleanup < gametic)
	{
		if(UseSndScript)
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].snd_ptr)
				{
					S_sfx[i].usefulness = -1;
				}
			}
		}
		else
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(S_sfx[i].usefulness == 0 && S_sfx[i].snd_ptr)
				{
					if(lumpcache[S_sfx[i].lumpnum])
					{
						if(((memblock_t *)((byte*)
							(lumpcache[S_sfx[i].lumpnum])-
							sizeof(memblock_t)))->id == 0x1d4a11)
						{ // taken directly from the Z_ChangeTag macro
							Z_ChangeTag2(lumpcache[S_sfx[i].lumpnum],
								PU_CACHE);
#ifdef __WATCOMC__
								_dpmi_unlockregion(S_sfx[i].snd_ptr,
									lumpinfo[S_sfx[i].lumpnum].size);
#endif
						}
					}
					S_sfx[i].usefulness = -1;
					S_sfx[i].snd_ptr = NULL;
				}
			}
		}
		NextCleanup = gametic+35*30; // every 30 seconds
	}
	for(i=0;i<snd_Channels;i++)
	{
		if(!Channel[i].handle || S_sfx[Channel[i].sound_id].usefulness == -1)
		{
			continue;
		}
		if(!I_SoundIsPlaying(Channel[i].handle))
		{
			if(S_sfx[Channel[i].sound_id].usefulness > 0)
			{
				S_sfx[Channel[i].sound_id].usefulness--;
			}
			Channel[i].handle = 0;
			Channel[i].mo = NULL;
			Channel[i].sound_id = 0;
		}
		if(Channel[i].mo == NULL || Channel[i].sound_id == 0
			|| Channel[i].mo == listener)
		{
			continue;
		}
		else
		{
			absx = abs(Channel[i].mo->x-listener->x);
			absy = abs(Channel[i].mo->y-listener->y);
			dist = absx+absy-(absx > absy ? absy>>1 : absx>>1);
			dist >>= FRACBITS;

			if(dist >= MAX_SND_DIST)
			{
				S_StopSound(Channel[i].mo);
				continue;
			}
			if(dist < 0)
			{
				dist = 0;
			}
			//vol = SoundCurve[dist];
			vol = (SoundCurve[dist]*(snd_MaxVolume*8)*Channel[i].volume)>>14;
			if(Channel[i].mo == listener)
			{
				sep = 128;
			}
			else
			{
				angle = R_PointToAngle2(listener->x, listener->y,
								Channel[i].mo->x, Channel[i].mo->y);
				angle = (angle-viewangle)>>24;
				sep = angle*2-128;
				if(sep < 64)
					sep = -sep;
				if(sep > 192)
					sep = 512-sep;
			}
			I_UpdateSoundParams(Channel[i].handle, vol, sep,
				Channel[i].pitch);
			priority = S_sfx[Channel[i].sound_id].priority;
			priority *= PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST);
			Channel[i].priority = priority;
		}
	}
}

//==========================================================================
//
// S_Init
//
//==========================================================================

void S_Init(void)
{
	SoundCurve = W_CacheLumpName("SNDCURVE", PU_STATIC);
//      SoundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);
	I_StartupSound();
	if(snd_Channels > 8)
	{
		snd_Channels = 8;
	}
	I_SetChannels(snd_Channels);
	I_SetMusicVolume(snd_MusicVolume);

	// Attempt to setup CD music
	if(snd_MusicDevice == snd_CDMUSIC)
	{
	   	ST_Message("    Attempting to initialize CD Music: ");
		if(0/*!cdrom*/)
		{
			i_CDMusic = 0;//(I_CDMusInit() != -1);
		}
		else
		{ // The user is trying to use the cdrom for both game and music
			i_CDMusic = false;
		}
	   	if(i_CDMusic)
	   	{
	   		ST_Message("initialized.\n");
	   	}
	   	else
	   	{
	   		ST_Message("failed.\n");
	   	}
	}
}

//==========================================================================
//
// S_GetChannelInfo
//
//==========================================================================

void S_GetChannelInfo(SoundInfo_t *s)
{
	int i;
	ChanInfo_t *c;

	s->channelCount = snd_Channels;
	s->musicVolume = snd_MusicVolume;
	s->soundVolume = snd_MaxVolume;
	for(i = 0; i < snd_Channels; i++)
	{
		c = &s->chan[i];
		c->id = Channel[i].sound_id;
		c->priority = Channel[i].priority;
		c->name = S_sfx[c->id].lumpname;
		c->mo = Channel[i].mo;
		c->distance = P_AproxDistance(c->mo->x-viewx, c->mo->y-viewy)
			>>FRACBITS;
	}
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
//==========================================================================

BOOLEAN S_GetSoundPlayingInfo(mobj_t *mobj, int sound_id)
{
	int i;

	for(i = 0; i < snd_Channels; i++)
	{
		if(Channel[i].sound_id == sound_id && Channel[i].mo == mobj)
		{
			if(I_SoundIsPlaying(Channel[i].handle))
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_SetMusicVolume
//
//==========================================================================

void S_SetMusicVolume(void)
{
	if(i_CDMusic)
	{
		//I_CDMusSetVolume(snd_MusicVolume*16); // 0-255
	}
	else
	{
		I_SetMusicVolume(snd_MusicVolume);
	}
	if(snd_MusicVolume == 0)
	{
		if(!i_CDMusic)
		{
			//I_PauseSong(RegisteredSong);
		}
		MusicPaused = true;
	}
	else if(MusicPaused)
	{
		if(!i_CDMusic)
		{
			//I_ResumeSong(RegisteredSong);
		}
		MusicPaused = false;
	}
}

//==========================================================================
//
// S_ShutDown
//
//==========================================================================

void S_ShutDown(void)
{
	extern int tsm_ID;
	if(tsm_ID != -1)
	{
		I_StopSong(RegisteredSong);
		I_UnRegisterSong(RegisteredSong);
		I_ShutdownSound();
	}
	if(i_CDMusic)
	{
		//I_CDMusStop();
	}
}
//==========================================================================
//
// S_InitScript
//
//==========================================================================

void S_InitScript(void)
{
	#if 1
	int p;
	int i;

	strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);
	if(!(p = M_CheckParm("-devsnd")))
	{
		UseSndScript = false;
		SC_OpenLump("sndinfo");
	}
	else
	{
		UseSndScript = true;
		SC_OpenFile(myargv[p+1]);
	}
	while(SC_GetString())
	{
		if(*sc_String == '$')
		{
			if(!stricmp(sc_String, "$ARCHIVEPATH"))
			{
				SC_MustGetString();
				strcpy(ArchivePath, sc_String);
			}
			else if(!stricmp(sc_String, "$MAP"))
			{
				SC_MustGetNumber();
				SC_MustGetString();
				if(sc_Number)
				{
					P_PutMapSongLump(sc_Number, sc_String);
				}
			}
			continue;
		}
		else
		{
			for(i = 0; i < NUMSFX; i++)
			{
				if(!strcmp(S_sfx[i].tagName, sc_String))
				{
					SC_MustGetString();
					if(*sc_String != '?')
					{
						strcpy(S_sfx[i].lumpname, sc_String);
					}
					else
					{
						strcpy(S_sfx[i].lumpname, "default");
					}
					break;
				}
			}
			if(i == NUMSFX)
			{
				SC_MustGetString();
			}
		}
	}
	SC_Close();

	for(i = 0; i < NUMSFX; i++)
	{
		if(!strcmp(S_sfx[i].lumpname, ""))
		{
			strcpy(S_sfx[i].lumpname, "default");
		}
	}
#endif
}

//==========================================================================
//
// I_UpdateCDMusic
//
// Updates playing time for current track, and restarts the track, if
// needed
//
//==========================================================================

void I_UpdateCDMusic(void)
{
	extern BOOLEAN MenuActive;

	if(MusicPaused || i_CDMusicLength < 0
	|| (paused && !MenuActive))
	{ // Non-looping song/song paused
		return;
	}
	i_CDMusicLength -= gametic-oldTic;
	oldTic = gametic;
	if(i_CDMusicLength <= 0)
	{
		S_StartSong(gamemap, true);
	}
}

/*
============================================================================

							CONSTANTS

============================================================================
*/

#define SC_INDEX                0x3C4
#define SC_RESET                0
#define SC_CLOCK                1
#define SC_MAPMASK              2
#define SC_CHARMAP              3
#define SC_MEMMODE              4


#define GC_INDEX                0x3CE
#define GC_SETRESET             0
#define GC_ENABLESETRESET 1
#define GC_COLORCOMPARE 2
#define GC_DATAROTATE   3
#define GC_READMAP              4
#define GC_MODE                 5
#define GC_MISCELLANEOUS 6
#define GC_COLORDONTCARE 7
#define GC_BITMASK              8




//==================================================
//
// joystick vars
//
//==================================================

BOOLEAN joystickpresent;
unsigned joystickx, joysticky;
         

BOOLEAN I_ReadJoystick (void) // returns false if not connected
{
    return false;
}

//==================================================

#define VBLCOUNTER              34000           // hardware tics to a frame

#define TIMERINT 8
#define KEYBOARDINT 9

#define MOUSEB1 1
#define MOUSEB2 2
#define MOUSEB3 4

BOOLEAN mousepresent;
//static  int tsm_ID = -1; // tsm init flag

//===============================

int ticcount;


BOOLEAN novideo; // if true, stay in text mode for debugging


//==========================================================================

//--------------------------------------------------------------------------
//
// FUNC I_GetTime
//
// Returns time in 1/35th second tics.
//
//--------------------------------------------------------------------------
#define ticks_to_msecs(ticks)      ((u32)((ticks)/(TB_TIMER_CLOCK)))

u32 gettick();
int I_GetTime (void)
{

   // ticcount = (SDL_GetTicks()*35)/1000;

   // struct timeval tv;
   // gettimeofday( &tv, 0 ); 

    //printf( "GT: %lx %lx\n", tv.tv_sec, tv.tv_usec );

 //   ticcount = ((tv.tv_sec * 1000000) + tv.tv_usec) / 28571;
  //  ticcount = ((tv.tv_sec / 35) + (tv.tv_usec / 28571));

    ticcount= (ticks_to_msecs(gettick()) / 35) & 0x7fffffff;
    return( ticcount );
}


/*
============================================================================

								USER INPUT

============================================================================
*/

//--------------------------------------------------------------------------
//
// PROC I_WaitVBL
//
//--------------------------------------------------------------------------

void I_WaitVBL(int vbls)
{
	if( novideo )
	{
		return;
	}


	while( vbls-- )
	{
     //   usleep( 16667000/1000 );
	}

}

//--------------------------------------------------------------------------
//
// PROC I_SetPalette
//
// Palette source must use 8 bit RGB elements.
//
//--------------------------------------------------------------------------

u32 paleta[256], paleta2[256];


void I_SetPalette(byte *palette)
{
int n;

usegamma=2;
//usegamma=4;
	for(n=0;n<256;n++)
		{
		paleta[n]= gammatable[usegamma][palette[0]]
			| (gammatable[usegamma][palette[1]]<<8) 
			| (gammatable[usegamma][palette[2]]<<16) | 0xff000000;
		
		paleta2[n]=paleta[n]; // menu palette

		palette+=3;
		}
// trick
paleta2[0]=0x00000000;
}

/*
============================================================================

							GRAPHICS MODE

============================================================================
*/

int wiimote_scr_info=0;

static int use_cheat=0;

extern byte *screen2;

extern BOOLEAN MenuActive;

void wii_scr_update()
{
	int n,m,l,ll;

	static int blink=0;

	I_StartFrame();

//	if(novideo) return;

    l=ll=0;

	for(n=0;n<SCREENHEIGHT; n++) 
	{
		for(m=0;m<SCREENWIDTH; m++)
			{
			wii_scr[l+m]=paleta[screen[ll+m]]; // used by game
			wii_scr2[l+m]=paleta2[screen2[ll+m]]; // used for draw menu
			}
		
		l+=TEXT_W;
		ll+=SCREENWIDTH;
	}

	memset((void *) screen2, 0, SCREENWIDTH*SCREENHEIGHT);

	CreateTexture(&text_scr, TILE_SRGBA8 , wii_scr, TEXT_W , TEXT_H, 0);

	SetTexture(&text_scr);

	DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, MenuActive ? 0xffa0a0a0 :0xffffffff); // reduce bright if Menu is active
	
	//Draw crosshair if using IR
	if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_NUNCHUK){
	SetTexture(NULL);
	DrawRoundFillBox(wmote_datas->ir.x, wmote_datas->ir.y, 20, 20, 0, 0xffffffff);
	}

	CreateTexture(&text_scr2, TILE_SRGBA8 , wii_scr2, TEXT_W , TEXT_H, 0);

	SetTexture(&text_scr2);

	DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0xffffffff);

	SetTexture(NULL);

	
	if(wiimote_scr_info & 1)
		{
		//Rewrite this with a proper Wii settings GUI later --Arikado
		
		/*SetTexture(NULL);
		DrawFillBox(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, (use_cheat>4) ? 0xff8f8f00 : 0xffff0000);

		PY=8;
		PX=8;
		letter_size(16, 32);
		autocenter=1;
		s_printf("Wiimote + Nunchuk info");
		autocenter=0;
		letter_size(12, 28);
		
		PY+=64;PX=8;
		s_printf("Nunchuk Stick -> To Move");
		
		PY+=32;PX=8;
		s_printf("Button HOME -> Menu");

		PY+=32;PX=8;
		s_printf("hold B with A -> Enter in Menu");
		
		PY+=32;PX=8;
		s_printf("Button A -> double click for Open/Use");
		
		PY+=32;PX=8;
		s_printf("Button B -> Fire, hold with A to Use Item");

		PY+=32;PX=8;
		s_printf("Button Z -> Run, hold with Wiimote L,R,U,D (Weapon)");
		
		PY+=32;PX=8;
		s_printf("Button C -> Jump, hold with Wiimote U,D,A (for fly)");

		PY+=32;PX=8;
		s_printf("Wiimote Left, Right -> Strafe (Except with Z)");
		
		PY+=32;PX=8;
		s_printf("Wiimote Up, Down -> Look Up/Down (Except with C & Z)");

		PY+=32;PX=8;
		s_printf("Button PLUS & MINUS -> Select Item");

		PY+=32;PX=8;
		s_printf("Button One -> Use Map");

		PY+=32;PX=8;
		s_printf("Button Two -> to see this info or exit");

		PY+=32;PX=8;
		s_printf("Note: L,R,U,D is for Left, Right, Up, Down");

		autocenter=1;
		PY+=32;PX=8;
		if(!((blink>>4) & 1)) s_printf("Press '2' to exit");
		blink++;
		autocenter=0;*/

		}
	




	Screen_flip();
}

/*
==============
=
= I_Update
=
==============
*/

int UpdateState;
extern int screenblocks;

void I_Update (void)
{
	int i;
	byte *dest;
	int tics;
	static int lasttic;
	int update=0;


	if(DisplayTicker)
	{
		if(/*screenblocks > 9 ||*/ UpdateState&(I_FULLSCRN|I_MESSAGES))
		{
			//dest = (byte *)screen;
			dest = (byte *) screen;
		}
		else
		{
			dest = (byte *) screen;
		}
		tics = ticcount-lasttic;
		lasttic = ticcount;
	
		if(tics > 20)
		{
			tics = 20;
		}
		for(i = 0; i < tics; i++)
		{
			*dest = 0xff;
			dest += 2;
		}
		for(i = tics; i < 20; i++)
		{
			*dest = 0x00;
			dest += 2;
		}
		
	}

	

	if(UpdateState == I_NOUPDATE)
	{
		return;
	}
	
	if(UpdateState&I_FULLSCRN)
	{
		//memcpy(pcscreen, screen, SCREENWIDTH*SCREENHEIGHT);
		UpdateState = I_NOUPDATE; // clear out all draw types

		update=1;

	}



	if(UpdateState&I_FULLVIEW)
	{
		if(UpdateState&I_MESSAGES && screenblocks > 7)
		{
			/*
			for(i = 0; i <
				(viewwindowy+viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen+i, screen+i, SCREENWIDTH);
			}*/
			UpdateState &= ~(I_FULLVIEW|I_MESSAGES);
			update=1;

          
		}
		else
		{
			/*
			for(i = viewwindowy*SCREENWIDTH+viewwindowx; i <
				(viewwindowy+viewheight)*SCREENWIDTH; i += SCREENWIDTH)
			{
				memcpy(pcscreen+i, screen+i, viewwidth);
			}
			*/
			UpdateState &= ~I_FULLVIEW;

			update=1;

		}
	
	}
	
	if(UpdateState&I_STATBAR)
	{
		/*memcpy(pcscreen+SCREENWIDTH*(SCREENHEIGHT-SBARHEIGHT),
			screen+SCREENWIDTH*(SCREENHEIGHT-SBARHEIGHT),
			SCREENWIDTH*SBARHEIGHT);*/
		UpdateState &= ~I_STATBAR;


		update=1;
	}
	if(UpdateState&I_MESSAGES)
	{
		//memcpy(pcscreen, screen, SCREENWIDTH*28);
		UpdateState &= ~I_MESSAGES;

	update=1;
	}

if(update) wii_scr_update();
}

//--------------------------------------------------------------------------
//
// PROC I_InitGraphics
//
//--------------------------------------------------------------------------

void I_InitGraphics(void)
{

	grabMouse = 0;


		I_SetPalette( W_CacheLumpName("PLAYPAL", PU_CACHE) );
}

//--------------------------------------------------------------------------
//
// PROC I_ShutdownGraphics
//
//--------------------------------------------------------------------------

void I_ShutdownGraphics(void)
{
}

//===========================================================================


//
// I_StartTic
//
void I_StartTic (void)
{

}


/*
===============
=
= I_StartFrame
=
===============
*/


#define J_DEATHZ 48 // death zone for sticks

extern void set_my_cheat(int indx);
BOOLEAN usergame;

void I_StartFrame (void)
{   
	event_t ev; // joystick event
	event_t event; // keyboard event

	int k_up=0,k_down=0,k_left=0,k_right=0,k_esc=0,k_enter=0,k_tab=0, k_del=0, k_pag_down=0;
	int k_jump=0,k_leftsel=0,k_rightsel=0,k_alt=0;
	int k_1=0,k_2=0,k_3=0,k_4=0,k_fly=0;

	static int w_jx=1,w_jy=1;

    PAD_ScanPads();
	s32 pad_stickx = PAD_StickX(0);
	s32 pad_sticky = PAD_StickY(0);
	s32 pad_substickx = PAD_SubStickX(0);
	s32 pad_substicky = PAD_SubStickY(0);
	
	WPAD_ScanPads();

	wiimote_read();
	WPAD_IR(WPAD_CHAN_0, &wmote_datas->ir);
	ev.type = ev_joystick;
	ev.data1 =  0;
	ev.data2 =  0;
	ev.data3 =  0;


	/*Cheat stuff

	if(new_pad & WPAD_BUTTON_A) {new_pad &=~ WPAD_BUTTON_A; set_my_cheat(4);wiimote_scr_info&=2;}//All Weapons
	if(new_pad & WPAD_BUTTON_PLUS) {new_pad &=~ WPAD_BUTTON_PLUS; set_my_cheat(2);wiimote_scr_info&=2;}//God Mode
	if(new_pad & WPAD_BUTTON_MINUS) {new_pad &=~ WPAD_BUTTON_MINUS; set_my_cheat(5);wiimote_scr_info&=2;}//Full Health
	if(new_pad & WPAD_BUTTON_1) {new_pad &=~ WPAD_BUTTON_1; set_my_cheat(6);wiimote_scr_info&=2;}//All Keys 
	if(new_pad & WPAD_BUTTON_2) {new_pad &=~ WPAD_BUTTON_2; set_my_cheat(9);wiimote_scr_info&=2;}//All Artifacts
			
	*/

	wiimote_scr_info&=1;

    //Wiimote + Nunchuk Controls
    if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_NUNCHUK)
	{
		// Menu
		if(new_pad & WPAD_BUTTON_HOME) k_esc=1;
        
		if(!MenuActive){
		// stick
		//Up
		if(wmote_datas->exp.nunchuk.js.pos.y> (wmote_datas->exp.nunchuk.js.center.y+J_DEATHZ))
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} else w_jy|=1;
		//Down
		if(wmote_datas->exp.nunchuk.js.pos.y< (wmote_datas->exp.nunchuk.js.center.y-J_DEATHZ))
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;} else w_jy|=2;
        //Left Strafe
		if((wmote_datas->exp.nunchuk.js.ang>=270-45 && wmote_datas->exp.nunchuk.js.ang<=270+45) && wmote_datas->exp.nunchuk.js.mag>=0.9)
			{k_left=1; k_alt=1;}
		//Right Strafe
		if((wmote_datas->exp.nunchuk.js.ang>=90-45 && wmote_datas->exp.nunchuk.js.ang<=90+45) && wmote_datas->exp.nunchuk.js.mag>=0.9) 
			{k_right=1;k_alt=1;}
		}
		
        if(!MenuActive){	
		//Turning via IR
		//Right
		if(wmote_datas->ir.x > 350)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if(wmote_datas->ir.x < 290)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if(wmote_datas->ir.y < 140)
			k_pag_down=1;
		//Down
		if(wmote_datas->ir.y > 340)
		    k_del=1;
		}
		
		if(MenuActive){
		if(WPAD_ButtonsDown(0)&WPAD_BUTTON_DOWN)
		{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;} else w_jy|=2;
		if(WPAD_ButtonsDown(0)&WPAD_BUTTON_UP)
		{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} else w_jy|=1;
		}
		
		if(old_pad & WPAD_NUNCHUK_BUTTON_Z) ev.data1|=4; // Run
		
		// Jump
		if(new_pad & WPAD_NUNCHUK_BUTTON_C) k_jump=1;

		// Map
		if(new_pad & WPAD_BUTTON_1) k_tab=1;

		// Select alphanumeric character (for naming saves)
		if(MenuActive){
		if(old_pad & WPAD_NUNCHUK_BUTTON_Z)
			{
			if(new_pad & WPAD_BUTTON_LEFT) k_1=1;
			if(new_pad & WPAD_BUTTON_UP) k_2=1;
			if(new_pad & WPAD_BUTTON_RIGHT) k_3=1;
			if(new_pad & WPAD_BUTTON_DOWN) k_4=1;
			}
		}
		
		//Change Weapon
		if(!MenuActive){
			if(new_pad & WPAD_BUTTON_LEFT) k_1=1;
			if(new_pad & WPAD_BUTTON_UP) k_2=1;
			if(new_pad & WPAD_BUTTON_RIGHT) k_3=1;
			if(new_pad & WPAD_BUTTON_DOWN) k_4=1;
		}
        
		if(!MenuActive){
		if(((new_pad & WPAD_BUTTON_A) && (old_pad & WPAD_BUTTON_B)))	
			{k_enter=1;}  // use object
		else
			{
			if(old_pad & WPAD_BUTTON_A) {ev.data1|=2;}  // open
			if(old_pad & WPAD_BUTTON_B) ev.data1|=1; // fire
			}
		}
		
		if(MenuActive){
		if(WPAD_ButtonsDown(0) & WPAD_BUTTON_A) {k_enter=1;}  // Select Option
		}

		if(new_pad & WPAD_BUTTON_MINUS) k_leftsel=1; // sel left object
		if(new_pad & WPAD_BUTTON_PLUS) k_rightsel=1; // sel right object

		H2_PostEvent (&ev);

		}
	//End Wiimote and Nunchuk Controls
	
	//Classic Controller Controls
	
		if(wmote_datas && wmote_datas->exp.type==WPAD_EXP_CLASSIC)
		{

		// Menu
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_HOME) k_esc=1;

		// Movement
		//Down
		if((wmote_datas->exp.classic.ljs.ang>=180-45 && wmote_datas->exp.classic.ljs.ang<=180+45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;}
        else w_jy|=2;
		//Up
		if((wmote_datas->exp.classic.ljs.ang>=360-45 || wmote_datas->exp.classic.ljs.ang<=45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} 
		else w_jy|=1;
		//Left Strafe
		if((wmote_datas->exp.classic.ljs.ang>=270-45 && wmote_datas->exp.classic.ljs.ang<=270+45) && wmote_datas->exp.classic.ljs.mag>=0.9)
			{k_left=1; k_alt=1;}
		//Right Strafe
		if((wmote_datas->exp.classic.ljs.ang>=90-45 && wmote_datas->exp.classic.ljs.ang<=90+45) && wmote_datas->exp.classic.ljs.mag>=0.9) 
			{k_right=1;k_alt=1;}
		
		//Turning
		//Right
		if((wmote_datas->exp.classic.rjs.ang>=90-45 && wmote_datas->exp.classic.rjs.ang<=90+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if((wmote_datas->exp.classic.rjs.ang>=270-45 && wmote_datas->exp.classic.rjs.ang<=270+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if((wmote_datas->exp.classic.rjs.ang>=360-45 || wmote_datas->exp.classic.rjs.ang<=45) && wmote_datas->exp.classic.rjs.mag>=0.9)
			k_pag_down=1;
		//Down
		if((wmote_datas->exp.classic.rjs.ang>=180-45 && wmote_datas->exp.classic.rjs.ang<=180+45) && wmote_datas->exp.classic.rjs.mag>=0.9)
		    k_del=1;

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_X || WPAD_ButtonsHeld(0)&WPAD_CLASSIC_BUTTON_X) ev.data1|=4; // Run

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_ZR) k_tab=1; //Toggle Map

		// Change weapon
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_LEFT)  k_1=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_RIGHT) k_2=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_UP)    k_3=1;
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_DOWN)  k_4=1;
		
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_Y) k_jump = 1; //Jump

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_ZL)	 {k_enter=1;}  // Use Object
		
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_A) {ev.data1|=2;}  // Open
		
		if((WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_B) || (WPAD_ButtonsHeld(0)&WPAD_CLASSIC_BUTTON_B)) ev.data1|=1; // Fire
			

		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_FULL_R) k_leftsel=1; // Select left object
		if(WPAD_ButtonsDown(0)&WPAD_CLASSIC_BUTTON_FULL_L) k_rightsel=1; // Select right object

		H2_PostEvent (&ev);

		}
	
	//End Classic Controller Controls
	
	
	//GC Controls
	
	if(wmote_datas->exp.type!=WPAD_EXP_CLASSIC && wmote_datas->exp.type!=WPAD_EXP_NUNCHUK)
	{
	
		// Menu
		if(PAD_ButtonsDown(0)&PAD_BUTTON_START) k_esc=1;

		// Movement
		//Down
		if(pad_sticky < -20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 2) k_down=1; w_jy&= ~2;ev.data3 = 1;}
        else w_jy|=2;
		//Up
		if(pad_sticky > 20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jy & 1) k_up=1; w_jy&= ~1; ev.data3 = -1;} 
		else w_jy|=1;
		if(pad_stickx < -20) {k_left=1; k_alt=1;} //Left Strafe
		if(pad_stickx > 20)  {k_right=1;k_alt=1;} // Right Strafe
		
		//Turning
		//Right
		if(pad_substickx > 20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 1) k_right=1; w_jx&= ~1; ev.data2 = 1;} 
		else w_jx|=1;
		//Left
		if(pad_substickx < -20)
			{time_sleep=TIME_SLEEP_SCR;SetVideoSleep(0);if(w_jx & 2) k_left=1; w_jx&= ~2;ev.data2 = -1;} 
		else w_jx|=2;
		//Up
		if(pad_substicky < -20) k_del=1;
		//Down
		if(pad_substicky > 20) k_pag_down=1;

		if(PAD_ButtonsDown(0)&PAD_TRIGGER_L || PAD_ButtonsHeld(0)&PAD_TRIGGER_L) ev.data1|=4; // Run

		if(PAD_ButtonsDown(0)&PAD_TRIGGER_Z) k_tab=1; //Toggle Map

		// Change weapon
		if(PAD_ButtonsDown(0)&PAD_BUTTON_LEFT)  k_1=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_RIGHT) k_2=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_UP)    k_3=1;
		if(PAD_ButtonsDown(0)&PAD_BUTTON_DOWN)  k_4=1;
		
		if(PAD_ButtonsDown(0)&PAD_BUTTON_Y) k_jump = 1;//Jump

		if(PAD_ButtonsDown(0)&PAD_BUTTON_X)	 {k_enter=1;}  // Use Object
		
		if(PAD_ButtonsDown(0)&PAD_BUTTON_B) {ev.data1|=2;}  // Open
		
		if((PAD_ButtonsDown(0)&PAD_BUTTON_A) || (PAD_ButtonsHeld(0)&PAD_BUTTON_A)) ev.data1|=1; // Fire
			

		if(PAD_ButtonsDown(0)&PAD_TRIGGER_R) k_leftsel=1; // Select left object
		//if(PAD_ButtonsDown(0)&PAD_TRIGGER_L) k_rightsel=1; // Select right object

		H2_PostEvent (&ev);
	
	}
	
	//End GC Controls

	// jump
	event.data1 = '/';
	if(k_jump) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_UPARROW;
	if(k_up) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_DOWNARROW;
	if(k_down) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_LEFTARROW;
	if(k_left) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_RIGHTARROW;
	if(k_right) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// strafe
	event.data1 = KEY_ALT;
	if(k_alt) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// enter
	event.data1 = KEY_ENTER;
	if(k_enter) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// menu
	event.data1 = KEY_ESCAPE;
	if(k_esc) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// mapa
	event.data1 = KEY_TAB;
	if(k_tab) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_LEFTBRACKET;
	if(k_leftsel) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	event.data1 = KEY_RIGHTBRACKET;
	if(k_rightsel) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	//  mira abajo
	event.data1 = KEY_DEL;
	if(k_del) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// mira arriba
	event.data1 = KEY_PGDN;
	if(k_pag_down) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// weapon
	event.data1 = '1';
	if(k_1) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);
	event.data1 = '2';
	if(k_2) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);
	event.data1 = '3';
	if(k_3) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);
	event.data1 = '4';
	if(k_4) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// fly up
	event.data1 = KEY_PGUP;
	if(k_fly==-1) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// fly down
	event.data1 = KEY_INS;
	if(k_fly==1) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

	// fly drop
	event.data1 = KEY_HOME;
	if(k_fly==-2) event.type = ev_keydown; else event.type = ev_keyup;
	H2_PostEvent(&event);

}


/*
============================================================================

							MOUSE

============================================================================
*/


/*
================
=
= StartupMouse
=
================
*/

void I_StartupCyberMan(void);

void I_StartupMouse (void)
{
	mousepresent = 0;

//	I_StartupCyberMan();
}




/*
===============
=
= I_StartupJoystick
=
===============
*/



void I_StartupJoystick (void)
{
// hermes

joystickpresent = true;

centerx = 0;
centery = 0;

}

/*
===============
=
= I_Init
=
= hook interrupts and set graphics mode
=
===============
*/

void I_Init (void)
{
	extern void I_StartupTimer(void);

	novideo = 0;
	
	I_StartupMouse();

	I_StartupJoystick();
	ST_Message("  S_Init... ");

	S_Init();
	S_Start();
}


/*
===============
=
= I_Shutdown
=
= return to default system state
=
===============
*/

void I_Shutdown (void)
{
	I_ShutdownGraphics ();
	S_ShutDown ();
}


/*
================
=
= I_Error
=
================
*/
char scr_str[256];

void I_Error (char *error, ...)
{
	va_list argptr;

	D_QuitNetGame ();
	I_Shutdown ();
	va_start (argptr,error);
	vsprintf (scr_str,error, argptr);
	va_end (argptr);
	
	PX=32;
	PY=16;
	letter_size(12, 24);
	s_printf ("%s\n", scr_str);
	Screen_flip();
	sleep(10);
	exit (1);
}

//--------------------------------------------------------------------------
//
// I_Quit
//
// Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//--------------------------------------------------------------------------

void I_Quit(void)
{
	D_QuitNetGame();
	M_SaveDefaults();
	I_Shutdown();

	exit(0);
}

/*
===============
=
= I_ZoneBase
=
===============
*/

byte *I_ZoneBase (int *size)
{
	static byte *ptr=NULL;
    int heap = 0x800000*2;

	extern void* SYS_AllocArena2MemLo(u32 size,u32 align);
  
	if(!ptr)
		{
		ptr =SYS_AllocArena2MemLo(heap,32);// malloc ( heap );

		ST_Message ("  0x%x allocated for zone, ", heap);
		ST_Message ("ZoneBase: 0x%X\n", (int)ptr);

		if ( ! ptr )
			I_Error ("  Insufficient DPMI memory!");

		memset(ptr, 255, heap);
		}

	*size = heap;
	
	return ptr;
}

/*
=============
=
= I_AllocLow
=
=============
*/

byte *I_AllocLow (int length)
{
	return malloc( length );
}

/*
============================================================================

						NETWORKING

============================================================================
*/

/* // FUCKED LINES
typedef struct
{
	char    priv[508];
} doomdata_t;
*/ // FUCKED LINES

#define DOOMCOM_ID              0x12345678l

/* // FUCKED LINES
typedef struct
{
	long    id;
	short   intnum;                 // DOOM executes an int to execute commands

// communication between DOOM and the driver
	short   command;                // CMD_SEND or CMD_GET
	short   remotenode;             // dest for send, set by get (-1 = no packet)
	short   datalength;             // bytes in doomdata to be sent

// info common to all nodes
	short   numnodes;               // console is allways node 0
	short   ticdup;                 // 1 = no duplication, 2-5 = dup for slow nets
	short   extratics;              // 1 = send a backup tic in every packet
	short   deathmatch;             // 1 = deathmatch
	short   savegame;               // -1 = new game, 0-5 = load savegame
	short   episode;                // 1-3
	short   map;                    // 1-9
	short   skill;                  // 1-5

// info specific to this node
	short   consoleplayer;
	short   numplayers;
	short   angleoffset;    // 1 = left, 0 = center, -1 = right
	short   drone;                  // 1 = drone

// packet data to be sent
	doomdata_t      data;
} doomcom_t;
*/ // FUCKED LINES

extern  doomcom_t               *doomcom;

/*
====================
=
= I_InitNetwork
=
====================
*/

void I_InitNetwork (void)
{
	int             i;

	i = M_CheckParm ("-net");
	if (!i)
	{
	//
	// single player game
	//
		doomcom = malloc (sizeof (*doomcom) );
		memset (doomcom, 0, sizeof(*doomcom) );
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->deathmatch = false;
		doomcom->consoleplayer = 0;
		doomcom->ticdup = 1;
		doomcom->extratics = 0;
		return;
	}

	netgame = true;
	doomcom = (doomcom_t *)atoi(myargv[i+1]);
//DEBUG
doomcom->skill = startskill;
doomcom->episode = startepisode;
doomcom->map = startmap;
doomcom->deathmatch = deathmatch;

}

void I_NetCmd (void)
{
	if (!netgame)
		I_Error ("I_NetCmd when not in netgame");
}


//==========================================================================
//
//
// I_StartupReadKeys
//
//
//==========================================================================

void I_StartupReadKeys(void)
{
   //if( KEY_ESCAPE pressed )
   //    I_Quit ();
}


//EOF
