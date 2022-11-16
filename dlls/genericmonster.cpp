/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Generic Monster - purely for scripted sequence work.
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"talkmonster.h"

// For holograms, make them not solid so the player can walk through them
//LRC- this seems to interfere with SF_MONSTER_CLIP
#define	SF_GENERICMONSTER_NOTSOLID					4 


#define	SF_HEAD_CONTROLLER					 8
#define SF_GENERICMONSTER_INVULNERABLE				32
//Not implemented:
#define SF_GENERICMONSTER_PLAYERMODEL					64


//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CGenericMonster : public CTalkMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int Classify( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int ISoundMask( void );

	void PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener );
	void IdleHeadTurn( Vector &vecFriend );
	void EXPORT MonsterThink();

	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_talkTime;
	EHANDLE m_hTalkTarget;
	float m_flIdealYaw;
	float m_flCurrentYaw;
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int HasCustomGibs( void ) { return m_iszGibModel; }

	int m_iszGibModel;
};
LINK_ENTITY_TO_CLASS( monster_generic, CGenericMonster );

TYPEDESCRIPTION	CGenericMonster::m_SaveData[] = 
{
	DEFINE_FIELD( CGenericMonster, m_iszGibModel, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CGenericMonster, CBaseMonster );

void CGenericMonster::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_bloodColor"))
	{
		m_bloodColor = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszGibModel"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}


TYPEDESCRIPTION CGenericMonster::m_SaveData[] =
		{
				DEFINE_FIELD( CGenericMonster, m_talkTime, FIELD_FLOAT ),
				DEFINE_FIELD( CGenericMonster, m_hTalkTarget, FIELD_EHANDLE ),
				DEFINE_FIELD( CGenericMonster, m_flIdealYaw, FIELD_FLOAT ),
				DEFINE_FIELD( CGenericMonster, m_flCurrentYaw, FIELD_FLOAT ),
		};

IMPLEMENT_SAVERESTORE( CGenericMonster, CBaseMonster )

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int CGenericMonster::Classify( void )
{
	return m_iClass?m_iClass:CLASS_PLAYER_ALLY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGenericMonster::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGenericMonster::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// ISoundMask - generic monster can't hear.
//=========================================================
int CGenericMonster::ISoundMask( void )
{
	return 0;
}

//=========================================================
// Spawn
//=========================================================
void CGenericMonster::Spawn()
{
	// store the size, so we can use it to set up the hulls after Set_Model overwrites it.
	Vector vecSize = pev->size;

	//LRC - if the level designer forgets to set a model, don't crash!
	if (FStringNull(pev->model))
	{
		if (pev->targetname)
			ALERT(at_error, "No model specified for monster_generic \"%s\"\n", STRING(pev->targetname));
		else
			ALERT(at_error, "No model specified for monster_generic at %.2f %.2f %.2f\n", pev->origin.x, pev->origin.y, pev->origin.z);
		pev->model = MAKE_STRING("models/player.mdl");
	}

	Precache();

	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	if (vecSize != g_vecZero)
	{
		Vector vecMax = vecSize/2;
		Vector vecMin = -vecMax;
		if (!FBitSet(pev->spawnflags,SF_GENERICMONSTER_PLAYERMODEL))
		{
			vecMin.z = 0;
			vecMax.z = vecSize.z;
		}
		UTIL_SetSize(pev, vecMin, vecMax);
	}
	else if (
			pev->spawnflags & SF_GENERICMONSTER_PLAYERMODEL ||
			FStrEq( STRING(pev->model), "models/player.mdl" ) ||
			FStrEq( STRING(pev->model), "models/holo.mdl" )
		)
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	if (!m_bloodColor) m_bloodColor = BLOOD_COLOR_RED;
	if (!pev->health) pev->health = 8;
	m_flFieldOfView = 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	if( pev->spawnflags & SF_HEAD_CONTROLLER )
	{
		m_afCapability = bits_CAP_TURN_HEAD;
	}
  
	m_flIdealYaw = m_flCurrentYaw = 0;

	if( pev->spawnflags & SF_GENERICMONSTER_NOTSOLID )
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}
	else if ( pev->spawnflags & SF_GENERICMONSTER_INVULNERABLE )
	{
		pev->takedamage = DAMAGE_NO;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGenericMonster::Precache()
{
	CTalkMonster::Precache();
	TalkInit();
	PRECACHE_MODEL( (char *)STRING(pev->model) );
	if (m_iszGibModel)
		PRECACHE_MODEL( STRING(m_iszGibModel) ); //LRC
}

void CGenericMonster::PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener )
{
	m_talkTime = gpGlobals->time + duration;
	PlaySentence( pszSentence, duration, volume, attenuation );

	m_hTalkTarget = pListener;
}

void CGenericMonster::IdleHeadTurn( Vector &vecFriend )
{
	// turn head in desired direction only if ent has a turnable head
	if( m_afCapability & bits_CAP_TURN_HEAD )
	{
		float yaw = VecToYaw( vecFriend - pev->origin ) - pev->angles.y;

		if( yaw > 180 )
			yaw -= 360;
		if( yaw < -180 )
			yaw += 360;

		m_flIdealYaw = yaw;
	}
}

void CGenericMonster::MonsterThink()
{
	if( m_afCapability & bits_CAP_TURN_HEAD )
	{
		if( m_hTalkTarget != 0 )
		{
			if( gpGlobals->time > m_talkTime )
			{
				m_flIdealYaw = 0;
				m_hTalkTarget = 0;
			}
			else
			{
				IdleHeadTurn( m_hTalkTarget->pev->origin );
			}
		}

		if( m_flCurrentYaw != m_flIdealYaw )
		{
			if( m_flCurrentYaw <= m_flIdealYaw )
			{
				m_flCurrentYaw += Q_min( m_flIdealYaw - m_flCurrentYaw, 20.0f );
			}
			else
			{
				m_flCurrentYaw -= Q_min( m_flCurrentYaw - m_flIdealYaw, 20.0f );
			}
			SetBoneController( 0, m_flCurrentYaw );
		}
	}

	CBaseMonster::MonsterThink();
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================


//=========================================================
// GENERIC DEAD MONSTER, PROP
//=========================================================
class CDeadGenericMonster : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int	Classify ( void ) { return CLASS_PLAYER_ALLY; }
	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int HasCustomGibs( void ) { return m_iszGibModel; }

	int m_iszGibModel;
};

LINK_ENTITY_TO_CLASS( monster_generic_dead, CDeadGenericMonster );

TYPEDESCRIPTION	CDeadGenericMonster::m_SaveData[] = 
{
	DEFINE_FIELD( CDeadGenericMonster, m_iszGibModel, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CDeadGenericMonster, CBaseMonster );

void CDeadGenericMonster::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_bloodColor"))
	{
		m_bloodColor = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszGibModel"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

//=========================================================
// ********** DeadGenericMonster SPAWN **********
//=========================================================
void CDeadGenericMonster :: Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));

	pev->effects		= 0;
	pev->yaw_speed		= 8; //LRC -- what?
	pev->sequence		= 0;

	if (pev->netname)
	{
		pev->sequence = LookupSequence( STRING(pev->netname) );

		if (pev->sequence == -1)
		{
			ALERT ( at_console, "Invalid sequence name \"%s\" in monster_generic_dead\n", STRING(pev->netname) );
		}
	}
	else
	{
		pev->sequence = LookupActivity( pev->frags );
//		if (pev->sequence == -1)
//		{
//			ALERT ( at_error, "monster_generic_dead - specify a sequence name or choose a different death type: model \"%s\" has no available death sequences.\n", STRING(pev->model) );
//		}
		//...and if that doesn't work, forget it.
	}

	// Corpses have less health
	pev->health			= 8;

	MonsterInitDead();

	ResetSequenceInfo( );
	pev->frame = 255; // pose at the _end_ of its death sequence.
}

void CDeadGenericMonster :: Precache()
{
	PRECACHE_MODEL( STRING(pev->model) );
	if (m_iszGibModel)
		PRECACHE_MODEL( STRING(m_iszGibModel) ); //LRC
}
