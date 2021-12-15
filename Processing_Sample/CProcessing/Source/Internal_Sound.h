//------------------------------------------------------------------------------
// file:	Internal_Sound.h
// author:	Daniel Hamilton, Andrea Ellinger
// brief:	Internal structs and functions for Sound
//
// INTERNAL USE ONLY, DO NOT DISTRIBUTE
//
// Copyright © 2019 DigiPen, All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Include Files:
//------------------------------------------------------------------------------

#include "fmod.h"

//------------------------------------------------------------------------------
// Defines:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Consts:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Structures:
//------------------------------------------------------------------------------

typedef struct CP_Sound_Struct
{
	char filepath[MAX_PATH];
    FMOD_SOUND* sound;
} CP_Sound_Struct;

typedef struct CP_Sound_DSP_Struct
{
	FMOD_DSP* dsp;
	CP_Sound_DSP_Param_Struct param[2];
} CP_Sound_DSP_Struct;

//------------------------------------------------------------------------------
// Public Enums:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Variables:
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Functions:
//------------------------------------------------------------------------------

void CP_Sound_Init(void);
void CP_Sound_Update(void);
void CP_Sound_Shutdown(void);

void CP_Sound_DSP_MapParameter(CP_SOUND_DSP dsp, CP_SOUND_DSP_PARAM param, int index, float min, float max);

#ifdef __cplusplus
}
#endif
