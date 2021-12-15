//------------------------------------------------------------------------------
// file:	CP_Sound.c
// author:	Daniel Hamilton, Andrea Ellinger, K Preston
// brief:	Load, play and manipulate sound files 
//
// Copyright © 2019 DigiPen, All rights reserved.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Include Files:
//------------------------------------------------------------------------------

#include "cprocessing.h"
#include "Internal_Sound.h"
#include "vect.h"

//------------------------------------------------------------------------------
// Defines and Internal Variables:
//------------------------------------------------------------------------------

#define MAX_FMOD_CHANNELS 128
#define CP_INITIAL_SOUND_CAPACITY   12
#define FMOD_TRUE 1
#define FMOD_FALSE 0

#define CP_SOUND_DSP_PARAM_NOTUSED -1

VECT_GENERATE_TYPE(CP_Sound)

static FMOD_RESULT result = 0;
static FMOD_SYSTEM* _fmod_system = NULL;

static vect_CP_Sound* sound_vector = NULL;

static FMOD_CHANNELGROUP* channel_groups[CP_SOUND_GROUP_MAX] = { NULL };
//static FMOD_DSP* dsp_list[CP_SOUND_DSP_MAX] = { NULL };

static CP_Sound_DSP_Struct dsp_list[CP_SOUND_DSP_MAX];

//------------------------------------------------------------------------------
// Internal Functions:
//------------------------------------------------------------------------------

static BOOL CP_IsValidSoundGroup(CP_SOUND_GROUP group)
{
	return group >= 0 && group < CP_SOUND_GROUP_MAX;
}

static CP_Sound CP_CheckIfSoundIsLoaded(const char* filepath)
{
	for (unsigned i = 0; i < sound_vector->size; ++i)
	{
		CP_Sound snd = vect_at_CP_Sound(sound_vector, i);
		if (snd && !strcmp(filepath, snd->filepath))
		{
			return snd;
		}
	}

	return NULL;
}

void CP_Sound_Init(void)
{
	// Allocate the initial vector size for loaded sounds
	sound_vector = vect_init_CP_Sound(CP_INITIAL_SOUND_CAPACITY);

	// Create the FMOD system
	result = FMOD_System_Create(&_fmod_system);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		printf("audio error");
		_fmod_system = NULL;
		return;
	}

	// Initialize the system
	result = FMOD_System_Init(_fmod_system, MAX_FMOD_CHANNELS, FMOD_INIT_NORMAL, NULL);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		_fmod_system = NULL;
		return;
	}

	// Create the channel groups (for stopping/pausing and controlling pitch and volume on a per group basis)
	for (unsigned index = 0; index < CP_SOUND_GROUP_MAX && result == FMOD_OK; ++index)
	{
		result = FMOD_System_CreateChannelGroup(_fmod_system, NULL, &channel_groups[index]);
	}
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}

	// Assign FMOD DSP effects within dsp_list array
	// Lowpass	| Parameter 1: = Cutoff Frequency	| Parameter 2 = Resonance
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_ITLOWPASS, &dsp_list[CP_SOUND_DSP_LOWPASS].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_LOWPASS, CP_SOUND_DSP_PARAM1, 0, 1, 22000);	// Cutoff
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_LOWPASS, CP_SOUND_DSP_PARAM2, 1, 0, 127);	// Resonance
	// Reverb	| Parameter 1: = Decay Time			| Parameter 2 = Wet Level
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_SFXREVERB, &dsp_list[CP_SOUND_DSP_REVERB].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_REVERB, CP_SOUND_DSP_PARAM1, 0, 100, 20000);	// Decay Time
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_REVERB, CP_SOUND_DSP_PARAM2, 11, -80, 20);	// Wet Level
	// Echo		| Parameter 1: = Delay Time			| Parameter 2 = Feedback
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_ECHO, &dsp_list[CP_SOUND_DSP_ECHO].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_ECHO, CP_SOUND_DSP_PARAM1, 0, 1, 5000);		// Decay Time
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_ECHO, CP_SOUND_DSP_PARAM2, 1, 0, 100);		// Wet Level
	// Distort	| Parameter 1: = Distortion Level	| Parameter 2 = [NOT USED]
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_DISTORTION, &dsp_list[CP_SOUND_DSP_DISTORT].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_DISTORT, CP_SOUND_DSP_PARAM1, 0, 0, 1);		// Distortion Level
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_DISTORT, CP_SOUND_DSP_PARAM2, CP_SOUND_DSP_PARAM_NOTUSED, 0, 0);
	// Flange	| Parameter 1: = Rate				| Parameter 2 = Mix
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_FLANGE, &dsp_list[CP_SOUND_DSP_FLANGE].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_FLANGE, CP_SOUND_DSP_PARAM1, 2, 0, 20);		// Rate
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_FLANGE, CP_SOUND_DSP_PARAM2, 0, 0, 100);		// Mix
	// Tremolo	| Parameter 1: = Frequency			| Parameter 2 = Depth
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_TREMOLO, &dsp_list[CP_SOUND_DSP_TREMOLO].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_TREMOLO, CP_SOUND_DSP_PARAM1, 0, 0.1f, 20);	// Frequency
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_TREMOLO, CP_SOUND_DSP_PARAM2, 1, 0, 1);		// Depth
	// Chorus	| Parameter 1: = Modulation Depth	| Parameter 2 = Mix
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_CHORUS, &dsp_list[CP_SOUND_DSP_CHORUS].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_CHORUS, CP_SOUND_DSP_PARAM1, 2, 0, 100);		// Modulation Depth
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_CHORUS, CP_SOUND_DSP_PARAM2, 0, 0, 100);		// Mix
	// Pitch	| Parameter 1: = Pitch				| Parameter 2 = [NOT USED]
	result = FMOD_System_CreateDSPByType(_fmod_system, FMOD_DSP_TYPE_PITCHSHIFT, &dsp_list[CP_SOUND_DSP_PITCH].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		CP_Sound_Shutdown();
		return;
	}
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_PITCH, CP_SOUND_DSP_PARAM1, 0, 0.5f, 2);		// Frequency
	CP_Sound_DSP_MapParameter(CP_SOUND_DSP_PITCH, CP_SOUND_DSP_PARAM2, CP_SOUND_DSP_PARAM_NOTUSED, 0, 0);
}

void CP_Sound_Update(void)
{
	if (_fmod_system != NULL)
	{
		result = FMOD_System_Update(_fmod_system);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)

			// Assume this is a fatal problem and shut down FMOD
			CP_Sound_Shutdown();
			return;
		}
	}
}

void CP_Sound_Shutdown(void)
{
	if (_fmod_system != NULL)
	{
		// Stop all current sounds
		CP_Sound_StopAll();

		// Free sounds 
		for (unsigned i = 0; i < sound_vector->size; ++i)
		{
			CP_Sound sound = vect_at_CP_Sound(sound_vector, i);
			// Release the sound from FMOD
			FMOD_Sound_Release(sound->sound);
			// Free the struct's memory
			free(sound);
		}

		// Free lists
		vect_free(sound_vector);

		// Release system
		FMOD_System_Release(_fmod_system);
		_fmod_system = NULL;
	}
}

CP_Sound CP_Sound_LoadInternal(const char* filepath, CP_BOOL streamFromDisc)
{
	if (!filepath)
		return NULL;

	CP_Sound sound = NULL;

	// Check if the sound is already loaded
	sound = CP_CheckIfSoundIsLoaded(filepath);
	if (sound)
	{
		return sound;
	}

	// Allocate memory for the struct
	sound = (CP_Sound)malloc(sizeof(CP_Sound_Struct));
	if (!sound)
	{
		// TODO handle error 
		return NULL;
	}

	// Create the FMOD sound
	if (streamFromDisc)
	{
		result = FMOD_System_CreateStream(_fmod_system, filepath, FMOD_DEFAULT, NULL, &(sound->sound));

	}
	else
	{
		result = FMOD_System_CreateSound(_fmod_system, filepath, FMOD_DEFAULT, NULL, &(sound->sound));
	}
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		free(sound);
		return NULL;
	}

	// Set filepath string for cache checking
	strcpy_s(sound->filepath, MAX_PATH, filepath);

	// Add it to the list
	vect_push_CP_Sound(sound_vector, sound);

	return sound;
}

void CP_Sound_DSP_MapParameter(CP_SOUND_DSP dsp, CP_SOUND_DSP_PARAM param, int index, float min, float max)
{
	dsp_list[dsp].param[param].index = index;
	dsp_list[dsp].param[param].min = min;
	dsp_list[dsp].param[param].max = max;
}


//------------------------------------------------------------------------------
// Library Functions:
//------------------------------------------------------------------------------

/*
	Load a CP_Sound by inputting the file path of the sound file as a string (const char*).
	Parameters:
		- filepath (const char*) - The path to the sound file that you want to load.
	Return:
		- CP_Sound - The loaded sound, returns a NULL CP_Sound if the sound could not be loaded.
*/
CP_API CP_Sound CP_Sound_Load(const char* filepath)
{
	return CP_Sound_LoadInternal(filepath, FALSE);
}

/*
	Loads a CP_Sound from the given file path, and streams the audio from disk while it is 
	playing instead of loading the entire file into memory.
	Parameters:
		- filepath (const char*) - The filepath to the music you want to load.
	Return:
		- CP_Sound - The music loaded from the given filepath, returns NULL if no music could be loaded.
*/
CP_API CP_Sound CP_Sound_LoadMusic(const char* filepath)
{
	return CP_Sound_LoadInternal(filepath, TRUE);
}

/*
	Frees a given CP_Sound from memory. The CP_Sound will not be valid after this call.
	Parameters:
		- sound (CP_Sound) - The sound you want to free.
*/
CP_API void CP_Sound_Free(CP_Sound* sound)
{
	if (sound == NULL || *sound == NULL)
	{
		return;
	}

	// Find the sound in the list
	for (unsigned i = 0; i < sound_vector->size; ++i)
	{
		// Check if this pointer is the same as the one we're looking for
		if (vect_at_CP_Sound(sound_vector, i) == *sound)
		{
			// Remove the sound from the list
			vect_rem_CP_Sound(sound_vector, i);
			// Release the sound from FMOD
			FMOD_Sound_Release((*sound)->sound);
			// Free the struct's memory
			free(*sound);
			*sound = NULL;
			return;
		}
	}

	// TODO: handle error - we reached the end of the list without finding the sound
}

/*
	Plays a given CP_Sound once in the CP_SOUND_GROUP_SFX sound group.
	Parameters:
		- sound (CP_Sound) - The sound you want to play.*/
CP_API void CP_Sound_Play(CP_Sound sound)
{
	CP_Sound_PlayAdvanced(sound, 1.0f, 1.0f, FALSE, CP_SOUND_GROUP_SFX);
}

/*
	Plays a given CP_Sound continuously in the CP_SOUND_GROUP_MUSIC sound group. 
	The sound will loop until it is stopped.
	Parameters:
		- sound (CP_Sound) - The sound you want to play as music.
	Return:
		- This function does not return anything.
*/
CP_API void CP_Sound_PlayMusic(CP_Sound sound)
{
	CP_Sound_PlayAdvanced(sound, 1.0f, 1.0f, TRUE, CP_SOUND_GROUP_MUSIC);
}

/*
	Plays a given CP_Sound with provided values for the sound's volume and pitch, 
	whether the sound will loop, and the sound group to play it in.
	Parameters:
		- sound (CP_Sound) - The sound that you want to play.
		- volume (float) - The volume modifier that you want to apply (1.0f = no modification, 0.0 = silent).
		- pitch (float) - The pitch modification that you want to apply (1.0f = no modification, 
			0.5 = half pitch, 2.0 = double pitch).
		- looping (P_BOOL) - If you want the sound to loop or not.
		- group (CP_SOUND_GROUP_MUSIC) - The sound group that you want the sound played in.
*/
CP_API void CP_Sound_PlayAdvanced(CP_Sound sound, float volume, float pitch, CP_BOOL looping, CP_SOUND_GROUP group)
{
	if (!CP_IsValidSoundGroup(group) || sound == NULL)
	{
		return;
	}

	// Only need to save the channel value temporarily since there is no channel-specific functionality
	FMOD_CHANNEL* channel;

	// Start the sound paused so we can set parameters on it
	result = FMOD_System_PlaySound(_fmod_system, sound->sound, channel_groups[group], FMOD_TRUE, &channel);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		return;
	}

	// Set the volume if it is not 1.0
	// (2.0 is double volume, 0.0 is silent)
	if (volume != 1.0f)
	{
		if (volume < 0.0f)
			volume = 0.0f;

		result = FMOD_Channel_SetVolume(channel, volume);
		// TODO: handle error - FMOD_ErrorString(result)
	}

	// Set the pitch if it is not 1.0
	// (0.5 is half pitch, 2.0 is double pitch)
	if (pitch != 1.0f)
	{
		if (pitch < 0.0f)
			pitch = 0.0f;

		result = FMOD_Channel_SetPitch(channel, pitch);
		// TODO: handle error - FMOD_ErrorString(result)
	}

	// Tell the sound to loop if the loop count is not zero
	// (-1 means loop infinitely, >0 makes it loop that many times then stop)
	if (looping)
	{
		result = FMOD_Channel_SetMode(channel, FMOD_LOOP_NORMAL);
		// TODO: handle error - FMOD_ErrorString(result)
		result = FMOD_Channel_SetLoopCount(channel, -1);
		// TODO: handle error - FMOD_ErrorString(result)
	}

	// Resume playing the sound
	result = FMOD_Channel_SetPaused(channel, FMOD_FALSE);
	// TODO: handle error - FMOD_ErrorString(result)
}

/*
	Pauses all CP_Sounds that are currently playing.
*/
CP_API void CP_Sound_PauseAll(void)
{
	for (unsigned index = 0; index < CP_SOUND_GROUP_MAX; ++index)
	{
		result = FMOD_ChannelGroup_SetPaused(channel_groups[index], FMOD_TRUE);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Pauses all CP_Sounds currently playing within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want to pause.
*/
CP_API void CP_Sound_PauseGroup(CP_SOUND_GROUP group)
{
	if(CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_SetPaused(channel_groups[group], FMOD_TRUE);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Resumes all CP_Sounds that are currently paused.
*/
CP_API void CP_Sound_ResumeAll(void)
{
	for (unsigned index = 0; index < CP_SOUND_GROUP_MAX; ++index)
	{
		result = FMOD_ChannelGroup_SetPaused(channel_groups[index], FMOD_FALSE);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Resumes all CP_Sounds that are currently paused within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want to resume playing.
*/
CP_API void CP_Sound_ResumeGroup(CP_SOUND_GROUP group)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_SetPaused(channel_groups[group], FMOD_FALSE);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Stops all currently playing CP_Sounds in all CP_SOUND_GROUPS and resets them to their beginnings.
*/
CP_API void CP_Sound_StopAll(void)
{
	for (unsigned index = 0; index < CP_SOUND_GROUP_MAX; ++index)
	{
		result = FMOD_ChannelGroup_Stop(channel_groups[index]);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Stops all CP_Sounds that are currently playing within a given group and 
	resets them to their beginnings.
	Parameters:
		- group (CP_SOUND_GROUP) - The group that you want to stop all sounds in.
*/
CP_API void CP_Sound_StopGroup(CP_SOUND_GROUP group)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_Stop(channel_groups[group]);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Sets the volume of all CP_Sounds within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want to set the volume for.
		- volume (float) - The volume modifier you want to apply to the group (1.0f is 
		normal volume, 0.0 is silent).
*/
CP_API void CP_Sound_SetGroupVolume(CP_SOUND_GROUP group, float volume)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_SetVolume(channel_groups[group], volume);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Gets the volume modifier of all CP_Sounds within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want the volume modifier of.
	Return:
		- float - The current volume modifier applied to all sounds within the given group.
*/
CP_API float CP_Sound_GetGroupVolume(CP_SOUND_GROUP group)
{
	float volume = 0;
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_GetVolume(channel_groups[group], &volume);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
	return volume;
}

/*
	Sets the pitch modifier of all CP_Sounds within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want to modify the pitch of.
		- pitch (float) - The pitch modifier that you want to give to all sounds in the given 
			group (1.0 is normal pitch, 0.5 is half pitch, 2.0 is double pitch).
*/
CP_API void CP_Sound_SetGroupPitch(CP_SOUND_GROUP group, float pitch)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_SetPitch(channel_groups[group], pitch);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Gets the pitch modifier applied to all CP_Sounds within the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group that you want to get the pitch of.
	Return:
		- float - The current pitch modifier of the specified CP_SOUND_GROUP.
*/
CP_API float CP_Sound_GetGroupPitch(CP_SOUND_GROUP group)
{
	float pitch = 0;
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_GetPitch(channel_groups[group], &pitch);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
	return pitch;
}

/*
	Connects input CP_SOUND_DSP to the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group to connect the DSP to.
		- dspType (CP_SOUND_DSP) - The DSP effect type to connect. 
*/
CP_API void CP_Sound_SetGroupDSP(CP_SOUND_GROUP group, CP_SOUND_DSP dspType)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_AddDSP(channel_groups[group], 0, dsp_list[dspType].dsp);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
		result = FMOD_DSP_SetActive(dsp_list[dspType].dsp, 1);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Removes all CP_SOUND_DSP connections from the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group to remove DSP connections from. 
*/
CP_API void CP_Sound_ClearGroupDSP(CP_SOUND_GROUP group)
{
	if (CP_IsValidSoundGroup(group))
	{
		for (CP_SOUND_DSP dsp = 0; dsp < CP_SOUND_DSP_MAX; dsp++)
		{
			result = FMOD_ChannelGroup_RemoveDSP(channel_groups[group], dsp_list[dsp].dsp);
			if (result != FMOD_OK)
			{
				// TODO: handle error - FMOD_ErrorString(result)
			}
		}
	}
}

/*
	Removes a specific CP_SOUND_DSP connection from the given CP_SOUND_GROUP.
	Parameters:
		- group (CP_SOUND_GROUP) - The sound group to remove DSP connections from.
		- dsp (CP_SOUND_DSP) - The DSP effect type to remove. 
*/
CP_API void CP_Sound_RemoveGroupDSP(CP_SOUND_GROUP group, CP_SOUND_DSP dsp)
{
	if (CP_IsValidSoundGroup(group))
	{
		result = FMOD_ChannelGroup_RemoveDSP(channel_groups[group], dsp_list[dsp].dsp);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
		}
	}
}

/*
	Sets the value of a given parameter of the specified CP_SOUND_DSP.
	Parameters:
		- dsp (CP_SOUND_DSP) - The DSP effect to modify.
		- parameter (CP_SOUND_DSP_PARAM) - The parameter to modify.
		- value (float) - The new parameter value to set.
*/
CP_API void CP_Sound_SetDSPParameter(CP_SOUND_DSP dsp, CP_SOUND_DSP_PARAM parameter, float value)
{
	if (dsp_list[dsp].param[parameter].index != CP_SOUND_DSP_PARAM_NOTUSED)
	{
		result = FMOD_DSP_SetParameterFloat(dsp_list[dsp].dsp,
			dsp_list[dsp].param[parameter].index,
			(dsp_list[dsp].param[parameter].max - dsp_list[dsp].param[parameter].min) * value +
			dsp_list[dsp].param[parameter].min);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
			return;
		}
	}
}

/*
	Resets given DSP internal state and parameters. 
	Parameters:
		- dsp (CP_SOUND_DSP) - The DSP effect to reset.
*/
CP_API void CP_Sound_ResetDSP(CP_SOUND_DSP dsp)
{
	result = FMOD_DSP_Reset(dsp_list[dsp].dsp);
	if (result != FMOD_OK)
	{
		// TODO: handle error - FMOD_ErrorString(result)
		return;
	}
}

/*
	Resets all DSP internal states and parameters.
*/
CP_API void CP_Sound_ResetAllDSPs()
{
	for (CP_SOUND_DSP dsp = 0; dsp < CP_SOUND_DSP_MAX; dsp++)
	{
		result = FMOD_DSP_Reset(dsp_list[dsp].dsp);
		if (result != FMOD_OK)
		{
			// TODO: handle error - FMOD_ErrorString(result)
			return;
		}
	}
}