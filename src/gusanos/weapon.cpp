#include "weapon.h"
#include "weapon_type.h"
#include "events.h"
#include "game/CWorm.h"
#include "gusgame.h"
#include "util/vec.h"
#include "util/angle.h"
#include "util/macros.h"
#include "util/log.h"
#include "timer_event.h"
#include "sprite_set.h"
#include "sprite.h"
#include "luaapi/context.h"
#include "network.h"
#include "game/CMap.h"

#include <iostream>

class CGameObject;

LuaReference Weapon::metaTable;

Weapon::Weapon(WeaponType* type, CWorm* owner)
{
	m_type = type;
	m_owner = owner;
	primaryShooting = false;
	ammo = type->ammo;
	inactiveTime = 0;
	reloading = false;
	reloadTime = 0;
	m_outOfAmmo = false;

	sentOutOfAmmo = false;
	outOfAmmoCheck = false;

	foreach(i, m_type->timer) {
		timer.push_back( (*i)->createState() );
	}

	foreach(i, m_type->activeTimer) {
		activeTimer.push_back( (*i)->createState() );
	}
}

Weapon::~Weapon()
{
}

void Weapon::reset()
{
	primaryShooting = false;
	ammo = m_type->ammo;
	inactiveTime = 0;
	reloading = false;
	reloadTime = 0;
	m_outOfAmmo = false;

	foreach(t, timer) {
		t->completeReset();
	}
	foreach(t, activeTimer) {
		t->completeReset();
	}
	foreach(t, shootTimer) {
		t->completeReset();
	}
}

void Weapon::think( bool isFocused, size_t index )
{
	foreach(t, timer) {
		if ( t->tick() ) {
			t->event->run(m_owner,0,0,this);
		}
	}

	if ( isFocused ) {
		foreach(t, activeTimer) {
			if ( t->tick() ) {
				t->event->run(m_owner,0,0,this);
			}
		}
	} else {
		foreach(t, activeTimer) {
			t->completeReset();
		}
	}

	if (inactiveTime > 0)
		inactiveTime--;
	else if ( isFocused ) {
		if ( primaryShooting && ammo > 0) {
			if (m_type->primaryShoot) {
				if ( m_owner->getRole() != eNet_RoleProxy || !m_type->syncHax ) {
					m_type->primaryShoot->run(m_owner, NULL, NULL, this );
					if ( m_owner->getRole() == eNet_RoleAuthority && m_type->syncHax ) {
						BitStream data;
						Encoding::encode(data, SHOOT, GameEventsCount);
						m_owner->sendWeaponMessage( index, &data, Net_REPRULE_AUTH_2_PROXY );
					}
				}
			}
		}
	}
	if ( isFocused ) {
		if ( ammo <= 0 && !reloading && !m_outOfAmmo) {
			m_outOfAmmo = true;
			//std::cout << "out of ammo" << endl;
			if ( !network.isClient() || !m_type->syncReload ) {
				outOfAmmo();

				if ( network.isHost() && m_type->syncReload ) {
					BitStream data;
					//data->addInt( OUTOFAMMO , 8);
					Encoding::encode(data, OUTOFAMMO, GameEventsCount);
					m_owner->sendWeaponMessage( index, &data );
					sentOutOfAmmo = true;
				}
			} else {
				BitStream data;
				Encoding::encode(data, OutOfAmmoCheck, GameEventsCount);
				m_owner->sendWeaponMessage( index, &data, Net_REPRULE_OWNER_2_AUTH );
				//std::cout << "sent check plz message" << endl;
			}
		}
		if ( reloading ) {
			if ( reloadTime > 0 ) {
				if((int)cClient->getGameLobby()[FT_LoadingTime] == 0)
					reloadTime = 0;
				else
					reloadTime -= 100.0 / (int)cClient->getGameLobby()[FT_LoadingTime];
				if(reloadTime < 0) reloadTime = 0;
			} else if ( !network.isClient() || !m_type->syncReload ) {
				reload();

				if ( network.isHost() && m_type->syncReload ) {
					BitStream data;
					//data->addInt( RELOADED , 8);
					Encoding::encode(data, RELOADED, GameEventsCount);
					m_owner->sendWeaponMessage( index, &data );
				}
			}
		}
	}

	if ( outOfAmmoCheck ) {
		//std::cout << "checking out of ammo" << endl;
		outOfAmmoCheck = false;
		if ( ammo > 0 ) {
			//std::cout << "Sending correction" << endl;
			BitStream data;
			Encoding::encode(data, AmmoCorrection, GameEventsCount);
			Encoding::encode(data, ammo, m_type->ammo+1);
			m_owner->sendWeaponMessage(index, &data, Net_REPRULE_AUTH_2_OWNER );
		} else {
			//std::cout << "Everything was in order" << endl;
			sentOutOfAmmo = false;
		}
	}
}

#ifndef DEDICATED_ONLY
void Weapon::drawBottom(ALLEGRO_BITMAP* where, int x, int y )
{
	if ( m_type->laserSightIntensity > 0 ) {
		Vec direction( m_owner->getPointingAngle() );
		Vec inc;
		float intensityInc = 0;
		if ( fabs(direction.x) > fabs(direction.y) ) {
			inc = direction / fabs(direction.x);
		} else {
			inc = direction / fabs(direction.y);
		}

		if ( m_type->laserSightRange > 0 ) {
			intensityInc = (float)(- (m_type->laserSightIntensity / m_type->laserSightRange) * inc.length());
		}
		Vec posDiff;
		float intensity = m_type->laserSightIntensity;
		while ( gusGame.level().getMaterial( (int)(m_owner->pos().x+posDiff.x), (int)(m_owner->pos().y+posDiff.y) ).particle_pass ) {
			if ( rnd() < intensity ) {
				if ( m_type->laserSightBlender != NONE )
					gfx.setBlender( m_type->laserSightBlender, m_type->laserSightAlpha );
				putpixel(where, (int)(posDiff.x)+x,(int)(posDiff.y)+y, m_type->laserSightColour);
				solid_mode();
			}
			posDiff+= inc;
			intensity+= intensityInc;
		}
	}
}

void Weapon::drawTop(ALLEGRO_BITMAP* where,int x, int y)
{
	if ( m_type->skin ) {
		m_type->skin->getSprite( 0, m_owner->getPointingAngle() )->draw(where, x, y);
	}
}
#endif

void Weapon::recieveMessage( BitStream* data )
{
	GameEvents event = static_cast<GameEvents>(Encoding::decode(*data, GameEventsCount));
	switch ( event ) {
			case OUTOFAMMO: {
				outOfAmmo();
			}
			break;

			case RELOADED: {
				reload();
			}
			break;

			case SHOOT: {
				m_type->primaryShoot->run(m_owner, NULL, NULL, this );
				ammo--;
			}
			break;

			case OutOfAmmoCheck: {
				outOfAmmoCheck = true;
			}
			break;

			case AmmoCorrection: {
				ammo = Encoding::decode(*data, m_type->ammo+1);
				m_outOfAmmo = false;
			}

			case GameEventsCount:
			break;
	}
}

void Weapon::outOfAmmo()
{
	if (m_type->outOfAmmo)
		m_type->outOfAmmo->run(m_owner, NULL, NULL, this );
	ammo = 0;
	if ( !reloading ) {
		reloading = true;
		reloadTime = m_type->reloadTime;
	}
}

void Weapon::reload()
{
	if (m_type->reloadEnd)
		m_type->reloadEnd->run(m_owner, NULL, NULL, this );
	reloading = false;
	ammo = m_type->ammo;
	m_outOfAmmo = false;
}

void Weapon::actionStart( Actions action )
{
	switch ( action ) {
			case PRIMARY_TRIGGER:
			if ( !inactiveTime && m_type->primaryPressed && ammo > 0 ) {
				m_type->primaryPressed->run( m_owner, NULL, NULL, this );
			}
			primaryShooting = true;
			break;

			case SECONDARY_TRIGGER:
			//TODO
			break;
	}
}

void Weapon::actionStop( Actions action )
{
	switch ( action ) {
			case PRIMARY_TRIGGER:
			if ( primaryShooting && m_type->primaryReleased ) {
				m_type->primaryReleased->run( m_owner, NULL, NULL, this);
			}
			primaryShooting = false;
			break;

			case SECONDARY_TRIGGER:
			//TODO
			break;
	}
}

CWorm* Weapon::getOwner()
{
	return m_owner;
}

void Weapon::delay( int time )
{
	inactiveTime = time;
}

void Weapon::useAmmo( int amount )
{
	ammo -= amount;
	if ( ammo < 0 )
		ammo = 0;
}

void Weapon::makeReference()
{
	lua.pushFullReference(*this, metaTable);
}

void Weapon::finalize()
{
	m_owner = 0;
	m_type = 0;
}
