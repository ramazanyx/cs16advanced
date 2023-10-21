
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "nodes.h"
#include "doors.h"
#include "subs.h"

/*
* Globals initialization
*/

// Global Savedata for Delay
TYPEDESCRIPTION CBaseDelay::m_SaveData[] =
{
	DEFINE_FIELD(CBaseDelay, m_flDelay, FIELD_FLOAT),
	DEFINE_FIELD(CBaseDelay, m_iszKillTarget, FIELD_STRING),
};

// Global Savedata for Toggle
TYPEDESCRIPTION CBaseToggle::m_SaveData[] =
{
	DEFINE_FIELD(CBaseToggle, m_toggle_state, FIELD_INTEGER),
	DEFINE_FIELD(CBaseToggle, m_flActivateFinished, FIELD_TIME),
	DEFINE_FIELD(CBaseToggle, m_flMoveDistance, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_flWait, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_flLip, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_flTWidth, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_flTLength, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_vecPosition1, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(CBaseToggle, m_vecPosition2, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(CBaseToggle, m_vecAngle1, FIELD_VECTOR),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD(CBaseToggle, m_vecAngle2, FIELD_VECTOR),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD(CBaseToggle, m_cTriggersLeft, FIELD_INTEGER),
	DEFINE_FIELD(CBaseToggle, m_flHeight, FIELD_FLOAT),
	DEFINE_FIELD(CBaseToggle, m_hActivator, FIELD_EHANDLE),
	DEFINE_FIELD(CBaseToggle, m_pfnCallWhenMoveDone, FIELD_FUNCTION),
	DEFINE_FIELD(CBaseToggle, m_vecFinalDest, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(CBaseToggle, m_vecFinalAngle, FIELD_VECTOR),
	DEFINE_FIELD(CBaseToggle, m_sMaster, FIELD_STRING),
	DEFINE_FIELD(CBaseToggle, m_bitsDamageInflict, FIELD_INTEGER),	// damage type inflicted
};

// Landmark class

void CPointEntity::Spawn()
{
	pev->solid = SOLID_NOT;
}

// Null Entity, remove on startup

void CNullEntity::Spawn()
{
	REMOVE_ENTITY(ENT(pev));
}

LINK_ENTITY_TO_CLASS(info_null, CNullEntity);

// These are the new entry points to entities.

LINK_ENTITY_TO_CLASS(info_player_deathmatch, CBaseDMStart);

LINK_ENTITY_TO_CLASS(info_player_start, CPointEntity);

LINK_ENTITY_TO_CLASS(info_vip_start, CBaseDMStart);

LINK_ENTITY_TO_CLASS(info_landmark, CPointEntity);

LINK_ENTITY_TO_CLASS(info_hostage_rescue, CPointEntity);

LINK_ENTITY_TO_CLASS(info_bomb_target, CPointEntity);

void CBaseDMStart::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

BOOL CBaseDMStart::IsTriggered(CBaseEntity *pEntity)
{
	BOOL master = UTIL_IsMasterTriggered(pev->netname, pEntity);

	return master;
}

// This updates global tables that need to know about entities being removed

void CBaseEntity::UpdateOnRemove()
{
	int i;

	if (pev->flags & FL_GRAPHED)
	{
		// this entity was a LinkEnt in the world node graph, so we must remove it from
		// the graph since we are removing it from the world.
		for (i = 0; i < WorldGraph.m_cLinks; ++i)
		{
			if (WorldGraph.m_pLinkPool[i].m_pLinkEnt == pev)
			{
				// if this link has a link ent which is the same ent that is removing itself, remove it!
				WorldGraph.m_pLinkPool[i].m_pLinkEnt = NULL;
			}
		}
	}

	if (pev->globalname)
	{
		gGlobalState.EntitySetState(pev->globalname, GLOBAL_DEAD);
	}
}

// Convenient way to delay removing oneself

void CBaseEntity::SUB_Remove()
{
	UpdateOnRemove();
	if (pev->health > 0)
	{
		// this situation can screw up monsters who can't tell their entity pointers are invalid.
		pev->health = 0;
		ALERT(at_aiconsole, "SUB_Remove called on entity with health > 0\n");
	}

	REMOVE_ENTITY(ENT(pev));
}

// Convenient way to explicitly do nothing (passed to functions that require a method)

void CBaseEntity::SUB_DoNothing()
{
	;
}

IMPLEMENT_SAVERESTORE(CBaseDelay, CBaseEntity);

void CBaseDelay::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{
		m_iszKillTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue(pkvd);
}

// If self.delay is set, a DelayedUse entity will be created that will actually
// do the SUB_UseTargets after that many seconds have passed.

// Removes all entities with a targetname that match self.killtarget,
// and removes them, so some events can remove other triggers.

// Search for (string)targetname in all entities that
// match (string)self.target and call their .use function (if they have one)

void CBaseEntity::SUB_UseTargets(CBaseEntity *pActivator, USE_TYPE useType, float value)
{
	// fire targets
	if (!FStringNull(pev->target))
	{
		FireTargets(STRING(pev->target), pActivator, this, useType, value);
	}
}

void FireTargets(const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	edict_t *pentTarget = NULL;
	if (!targetName)
		return;

	ALERT(at_aiconsole, "Firing: (%s)\n", targetName);
	while (true)
	{
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, targetName);
		if (FNullEnt(pentTarget))
			break;

		CBaseEntity *pTarget = CBaseEntity::Instance(pentTarget);

		// Don't use dying ents
		if (pTarget && !(pTarget->pev->flags & FL_KILLME))
		{
			ALERT(at_aiconsole, "Found: %s, firing (%s)\n", STRING(pTarget->pev->classname), targetName);
			pTarget->Use(pActivator, pCaller, useType, value);
		}
	}
}

LINK_ENTITY_TO_CLASS(DelayedUse, CBaseDelay);

void CBaseDelay::SUB_UseTargets(CBaseEntity *pActivator, USE_TYPE useType, float value)
{
	// exit immediatly if we don't have a target or kill target
	if (FStringNull(pev->target) && !m_iszKillTarget)
		return;

	// check for a delay
	if (m_flDelay != 0)
	{
		// create a temp object to fire at a later time
		CBaseDelay *pTemp = GetClassPtr((CBaseDelay *)NULL);
		if (pTemp->pev->classname)
		{
			RemoveEntityHashValue(pTemp->pev, STRING(pTemp->pev->classname), CLASSNAME);
		}

		MAKE_STRING_CLASS("DelayedUse", pTemp->pev);
		AddEntityHashValue(pTemp->pev, STRING(pTemp->pev->classname), CLASSNAME);

		pTemp->pev->nextthink = gpGlobals->time + m_flDelay;
		pTemp->SetThink(&CBaseDelay::DelayThink);

		// Save the useType
		pTemp->pev->button = (int)useType;
		pTemp->m_iszKillTarget = m_iszKillTarget;

		// prevent "recursion"
		pTemp->m_flDelay = 0;
		pTemp->pev->target = pev->target;

		// HACKHACK
		// This wasn't in the release build of Half-Life.  We should have moved m_hActivator into this class
		// but changing member variable hierarchy would break save/restore without some ugly code.
		// This code is not as ugly as that code

		// If a player activates, then save it
		if (pActivator && pActivator->IsPlayer())
		{
			pTemp->pev->owner = pActivator->edict();
		}
		else
		{
			pTemp->pev->owner = NULL;
		}

		return;
	}

	// kill the killtargets
	if (m_iszKillTarget)
	{
		edict_t *pentKillTarget = NULL;

		ALERT(at_aiconsole, "KillTarget: %s\n", STRING(m_iszKillTarget));
		pentKillTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_iszKillTarget));

		while (!FNullEnt(pentKillTarget))
		{
			UTIL_Remove(CBaseEntity::Instance(pentKillTarget));

			ALERT(at_aiconsole, "killing %s\n", STRING(pentKillTarget->v.classname));
			pentKillTarget = FIND_ENTITY_BY_TARGETNAME(pentKillTarget, STRING(m_iszKillTarget));
		}
	}

	// fire targets
	if (!FStringNull(pev->target))
	{
		FireTargets(STRING(pev->target), pActivator, this, useType, value);
	}
}

// QuakeEd only writes a single float for angles (bad idea), so up and down are
// just constant angles.

void SetMovedir(entvars_t *pev)
{
	if (pev->angles == Vector(0, -1, 0))
	{
		pev->movedir = Vector(0, 0, 1);
	}
	else if (pev->angles == Vector(0, -2, 0))
	{
		pev->movedir = Vector(0, 0, -1);
	}
	else
	{
		UTIL_MakeVectors(pev->angles);
		pev->movedir = gpGlobals->v_forward;
	}

	pev->angles = g_vecZero;
}

void CBaseDelay::DelayThink()
{
	CBaseEntity *pActivator = NULL;

	// A player activated this on delay
	if (pev->owner != NULL)
	{
		pActivator = CBaseEntity::Instance(pev->owner);
	}

	// The use type is cached (and stashed) in pev->button
	SUB_UseTargets(pActivator, (USE_TYPE)pev->button, 0);
	REMOVE_ENTITY(ENT(pev));
}

IMPLEMENT_SAVERESTORE(CBaseToggle, CBaseAnimating);

void CBaseToggle::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

// calculate pev->velocity and pev->nextthink to reach vecDest from
// pev->origin traveling at flSpeed

void CBaseToggle::LinearMove(Vector vecDest, float flSpeed)
{
	assert(("LinearMove:  no speed is defined!", flSpeed != 0));
	//assert(("LinearMove: no post-move function defined", m_pfnCallWhenMoveDone != NULL));

	m_vecFinalDest = vecDest;

	// Already there?
	if (vecDest == pev->origin)
	{
		LinearMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - pev->origin;

	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink(&CBaseToggle::LinearMoveDone);

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->velocity = vecDestDelta * (float)(1 / flTravelTime);
}

// After moving, set origin to exact final destination, call "move done" function

void CBaseToggle::LinearMoveDone()
{
	UTIL_SetOrigin(pev, m_vecFinalDest);
	pev->velocity = g_vecZero;
	pev->nextthink = -1;

	if (m_pfnCallWhenMoveDone)
	{
		(this->*m_pfnCallWhenMoveDone)();
	}
}

NOXREF BOOL CBaseToggle::IsLockedByMaster()
{
	if (!FStringNull(m_sMaster) && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return TRUE;
	else
		return FALSE;
}

// calculate pev->velocity and pev->nextthink to reach vecDest from
// pev->origin traveling at flSpeed
// Just like LinearMove, but rotational.

void CBaseToggle::AngularMove(Vector vecDestAngle, float flSpeed)
{
	assert(("AngularMove:  no speed is defined!", flSpeed != 0));
	//assert(("AngularMove: no post-move function defined", m_pfnCallWhenMoveDone != NULL));

	m_vecFinalAngle = vecDestAngle;

	// Already there?
	if (vecDestAngle == pev->angles)
	{
		AngularMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - pev->angles;

	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink(&CBaseToggle::AngularMoveDone);

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->avelocity = vecDestDelta / flTravelTime;
}

// After rotating, set angle to exact final angle, call "move done" function

void CBaseToggle::AngularMoveDone()
{
	pev->angles = m_vecFinalAngle;
	pev->avelocity = g_vecZero;
	pev->nextthink = -1;

	if (m_pfnCallWhenMoveDone)
	{
		(this->*m_pfnCallWhenMoveDone)();
	}
}

float CBaseToggle::AxisValue(int flags, const Vector &angles)
{
	if (flags & SF_DOOR_ROTATE_Z)
		return angles.z;

	if (flags & SF_DOOR_ROTATE_X)
		return angles.x;

	return angles.y;
}

void CBaseToggle::AxisDir(entvars_t *pev)
{
	if (pev->spawnflags & SF_DOOR_ROTATE_Z)
	{
		// around z-axis
		pev->movedir = Vector(0, 0, 1);
	}
	else if (pev->spawnflags & SF_DOOR_ROTATE_X)
	{
		// around x-axis
		pev->movedir = Vector(1, 0, 0);
	}
	else
	{
		// around y-axis
		pev->movedir = Vector(0, 1, 0);
	}
}

float CBaseToggle::AxisDelta(int flags, const Vector &angle1, const Vector &angle2)
{
	if (flags & SF_DOOR_ROTATE_Z)
		return angle1.z - angle2.z;

	if (flags & SF_DOOR_ROTATE_X)
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}

// returns TRUE if the passed entity is visible to caller, even if not infront ()

NOXREF BOOL FEntIsVisible(entvars_t *pev, entvars_t *pevTarget)
{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	TraceResult tr;

	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr);

	if (tr.fInOpen && tr.fInWater)
	{
		// sight line crossed contents
		return FALSE;
	}

	if (tr.flFraction == 1.0f)
	{
		return TRUE;
	}

	return FALSE;
}
