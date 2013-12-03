/*
    C-Dogs SDL
    A port of the legendary (and fun) action/arcade cdogs.
    Copyright (C) 1995 Ronny Wester
    Copyright (C) 2003 Jeremy Chin
    Copyright (C) 2003-2007 Lucas Martin-King

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

-------------------------------------------------------------------------------

 triggers.c - trigger related functions

 Author: $Author$
 Rev:    $Revision$
 URL:    $HeadURL$
 ID:     $Id$

*/

#include <stdlib.h>
#include <string.h>
#include "triggers.h"
#include "map.h"
#include "sounds.h"
#include "utils.h"

static TTrigger *root = NULL;
static TWatch *activeWatches = NULL;
static TWatch *inactiveWatches = NULL;
static int watchIndex = 1;


static TAction *AddActions(int count)
{
	TAction *a;
	CCALLOC(a, sizeof(TAction) * (count + 1));
	return a;
}

TTrigger *AddTrigger(int x, int y, int actionCount)
{
	TTrigger *t;
	TTrigger **h;

	CCALLOC(t, sizeof(TTrigger));
	t->x = x;
	t->y = y;

	h = &root;
	while (*h) {
		if ((*h)->y < y || ((*h)->y == y && (*h)->x < x))
			h = &((*h)->right);
		else
			h = &((*h)->left);
	}
	*h = t;
	t->actions = AddActions(actionCount);
	return t;
}

void FreeTrigger(TTrigger * t)
{
	if (!t)
		return;

	FreeTrigger(t->left);
	FreeTrigger(t->right);
	CFREE(t->actions);
	CFREE(t);
}

static int RemoveAllTriggers(void)
{
	FreeTrigger(root);
	root = NULL;

	return 0;
}

static TCondition *AddConditions(int count)
{
	TCondition *a;
	CCALLOC(a, sizeof(TCondition) * (count + 1));
	return a;
}

TWatch *AddWatch(int conditionCount, int actionCount)
{
	TWatch *t;
	CCALLOC(t, sizeof(TWatch));
	t->index = watchIndex++;
	t->next = inactiveWatches;
	inactiveWatches = t;
	t->actions = AddActions(actionCount);
	t->conditions = AddConditions(conditionCount);
	return t;
}

static TWatch *FindWatch(int idx)
{
	TWatch *t = inactiveWatches;

	while (t && t->index != idx)
	{
		t = t->next;
	}
	return t;
}

static void ActivateWatch(int idx)
{
	TWatch **h = &inactiveWatches;
	TWatch *t;

	while (*h && (*h)->index != idx)
	{
		h = &((*h)->next);
	}
	if (*h)
	{
		t = *h;
		*h = t->next;
		t->next = activeWatches;
		activeWatches = t;
	}
}

static void DeactivateWatch(int idx)
{
	TWatch **h = &activeWatches;
	TWatch *t;

	while (*h && (*h)->index != idx)
	{
		h = &((*h)->next);
	}
	if (*h)
	{
		t = *h;
		*h = t->next;
		t->next = inactiveWatches;
		inactiveWatches = t;
	}
}

static void RemoveAllWatches(void)
{
	TWatch *t;

	while (activeWatches) {
		t = activeWatches;
		activeWatches = t->next;
		CFREE(t->conditions);
		CFREE(t->actions);
		CFREE(t);
	}
	while (inactiveWatches) {
		t = inactiveWatches;
		inactiveWatches = t->next;
		CFREE(t->conditions);
		CFREE(t->actions);
		CFREE(t);
	}
}

void FreeTriggersAndWatches(void)
{
	RemoveAllTriggers();
	RemoveAllWatches();
}

static void Action(TAction * a)
{
	TWatch *t;
	TCondition *c;

	for (;;)
	{
		switch (a->action)
		{
		case ACTION_NULL:
			return;

		case ACTION_SOUND:
			SoundPlayAt(&gSoundDevice, a->tileFlags, Vec2iNew(a->x, a->y));
			break;

		case ACTION_SETTRIGGER:
			Map(a->x, a->y).flags |= MAPTILE_TILE_TRIGGER;
			break;

		case ACTION_CLEARTRIGGER:
			Map(a->x, a->y).flags &= ~MAPTILE_TILE_TRIGGER;
			break;

		case ACTION_CHANGETILE:
			Map(a->x, a->y).flags = a->tileFlags;
			Map(a->x, a->y).pic = a->tilePic;
			Map(a->x, a->y).picAlt = a->tilePicAlt;
			break;

		case ACTION_SETTIMEDWATCH:
			t = FindWatch(a->x);
			if (t) {
				c = t->conditions;
				while (c && c->condition != CONDITION_NULL) {
					if (c->condition ==
					    CONDITION_TIMEDDELAY) {
						c->x = a->y;
						break;
					}
					c++;
				}
				ActivateWatch(t->index);
			}
			break;

		case ACTION_ACTIVATEWATCH:
			ActivateWatch(a->x);
			break;

		case ACTION_DEACTIVATEWATCH:
			DeactivateWatch(a->x);
			break;
		}
		a++;
	}
}

static int ConditionMet(TCondition *c)
{
	for (;;)
	{
		switch (c->condition)
		{
		case CONDITION_NULL:
			return 1;

		case CONDITION_TIMEDDELAY:
			c->x--;
			if (c->x > 0)
				return 0;
			break;

		case CONDITION_TILECLEAR:
			if (Map(c->x, c->y).things != NULL)
				return 0;
			break;
		}
		c++;
	}
}

static TTrigger *FindTrigger(TTrigger *t, Vec2i pos)
{
	if (!t)
		return NULL;

	if (t->x == pos.x && t->y == pos.y)
	{
		return t;
	}
	if (pos.y > t->y || (pos.y == t->y && pos.x > t->x))
	{
		return FindTrigger(t->right, pos);
	}
	return FindTrigger(t->left, pos);
}

void TriggerAt(Vec2i pos, int flags)
{
	TTrigger *t = FindTrigger(root, pos);
	while (t)
	{
		if (t->flags == 0 || (t->flags & flags))
		{
			Action(t->actions);
		}
		t = FindTrigger(t->left, pos);
	}
}

void UpdateWatches(void)
{
	TWatch *a = activeWatches;
	TWatch *current;

	while (a) {
		current = a;
		a = a->next;
		if (ConditionMet(current->conditions))
			Action(current->actions);
	}
}
