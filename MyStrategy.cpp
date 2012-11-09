﻿#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

using namespace model;
using namespace std;

const double MIN_ANGLE = M_PI / 6.0; 
const double MIN_GOTO_ANGLE = M_PI / 12.0;
const double MIN_FIRING_ANGLE = M_PI / 90.0;
const double MIN_DANGER_LEVEL = 0.8;

bool GoTo (model::Move& move, double angle)
{
	double ftangs = tan (fabs (angle));
	double tangs = tan (angle);
	if (ftangs <= 1/sqrt (3.0) && ftangs >= -1/sqrt (3.0))
	{
		if (cos (angle) >= 0)
		{	//еду вперёд
			move.set_left_track_power  (1.0);
			move.set_right_track_power (1.0);
		}
		else
		{	//еду назад
			move.set_left_track_power  (-1.0);
			move.set_right_track_power (-1.0);
		}
	}
	else if (tangs >= 0)
	{	//поворот направо
		move.set_left_track_power  (1.0);
		move.set_right_track_power (-1.0);
	}
	else
	{	//поворот налево
		move.set_left_track_power  (-1.0);
		move.set_right_track_power (1.0);
	}
	return true;
}
bool GoFrom (model::Move& move, double angle, double angle_to_go = 0)
{
	double ftangs = tan (fabs (angle));
	double tangs = tan (angle);
	if (ftangs >= sqrt (3.0) || ftangs <= -sqrt (3.0))
	{
		if (cos (angle_to_go) >= 0)
		{	//еду вперёд
			move.set_left_track_power  (1.0);
			move.set_right_track_power (1.0);
		}
		else
		{	//еду назад
			move.set_left_track_power  (-1.0);
			move.set_right_track_power (-1.0);
		}
	}
	else if (tangs <= 0)
	{	//поворот направо
		move.set_left_track_power  (1.0);
		move.set_right_track_power (-1.0);
	}
	else
	{	//поворот налево
		move.set_left_track_power  (-1.0);
		move.set_right_track_power (1.0);
	}
	return true;
}

double MeasureSelf_Health (Tank &self, World &world)
{
	double health = self.crew_health() / self.crew_max_health();
	if (health <= 0.25)
		return 1;
	//if (health <= 0.7)
	//	return 1 - health;
	return 1 - health;	//сюда бы квадрат
}

double MeasureSelf_Shield (Tank &self, World &world)
{
	double shield = self.hull_durability() / self.hull_max_durability();
	if (self.hull_durability() <= 25)
		return 1;
	//if (shield <= 0.5)
	//	return 1 - shield;
	return 1 - shield;	//сюда бы квадрат
}

double MeasureSelf_Ammo (Tank &self, World &world)
{
	double ammo = self.premium_shell_count();
	if (ammo == 0)
		return 0.3;
	return 0.3 / ammo;
}

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	vector <Tank> all_tanks = world.tanks();
	vector <Shell> shells = world.shells();
	vector <Bonus> bonuses = world.bonuses();
	move.set_fire_type(NONE);
	vector <Tank> EnemiesToAttack;
	//заполнение массива живых врагов. + вычисление max_dist_alive
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health()) && (all_tanks[i].hull_durability()) && (!all_tanks[i].teammate()) && (!(all_tanks[i].player_name() == "EmptyPlayer")) && (!(all_tanks[i].player_name().substr (0,9) == "Akhrameev")))
			EnemiesToAttack.push_back (all_tanks[i]);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////FIRING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	Tank *enemy = NULL;
	double measure_enemy = 0;
	double max_dist_alive_enemy = 0;
	double max_angle_alive_enemy = 0;
	for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
	{
		double dist = self.GetDistanceTo(EnemiesToAttack[i]);
		double angle = fabs (self.GetTurretAngleTo(EnemiesToAttack[i]));
		if (dist > max_dist_alive_enemy)
			max_dist_alive_enemy = dist;
		if (angle > max_angle_alive_enemy)
			max_angle_alive_enemy = angle;
	}
	if (max_angle_alive_enemy != 0 && max_dist_alive_enemy != 0)
	{
		for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
		{
			//double measure = MeasureEnemy_ToAtack (&self, &world, &(all_tanks[EnemiesToAttack[i]]));
			Tank enemy_obj = EnemiesToAttack[i];

			double health_measure = 0;
			if (enemy_obj.crew_health() != 0)
				health_measure = 1 - 0.8 * enemy_obj.crew_health() / enemy_obj.crew_max_health();
			if (enemy_obj.crew_health() <= 25 && enemy_obj.crew_health() > 0)
				health_measure = 1;
			double shield_measure = 0;
			if (enemy_obj.hull_durability() != 0)
				shield_measure = 1 - 0.8 * enemy_obj.hull_durability() / enemy_obj.hull_max_durability();
			if (enemy_obj.hull_durability() <= 25 && enemy_obj.hull_durability() > 0)
				shield_measure = 1;
			double measure = health_measure * shield_measure * (1 - pow (0.8 * fabs (self.GetTurretAngleTo (enemy_obj)) / max_angle_alive_enemy, 3.7)) * (1 - pow (0.8 * (self.GetDistanceTo (enemy_obj) / max_dist_alive_enemy), 1.9));
			if (measure > measure_enemy)
			{
				enemy = &EnemiesToAttack[i];
				measure_enemy = measure;
			}
		}
	}
	if (enemy)
	{		//определил врага
		if (self.remaining_reloading_time () / self.reloading_time() <= 0.7)	//UP
		{
			double angle = self.GetTurretAngleTo (*enemy);
			double dist = self.GetDistanceTo (*enemy);
			if (fabs (angle) <= MIN_FIRING_ANGLE)
			{
				if (self.remaining_reloading_time() == 0)
				{
					if (dist * fabs (sin (angle)) <= enemy->height()/2 && dist <= world.height()/2)
						move.set_fire_type (PREMIUM_PREFERRED);
					else
						move.set_fire_type (REGULAR_FIRE);
				}
			}
			else if (angle >= 0)
				move.set_turret_turn (self.turret_turn_speed());
			else
				move.set_turret_turn (-self.turret_turn_speed());
			/*if ((double (self.remaining_reloading_time()) / self.reloading_time() < 0.05) && (fabs (angle) >= MIN_FIRING_ANGLE))
			{
				double right = 1;
				if (angle < 0)
					right = -1;
				move.set_left_track_power  (right);
				move.set_right_track_power (-right);
				if (fabs (angle) <= HELP_FIRING_ANGLE)
					fire_on = true;
			}*/
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////MOVING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////

	if (world.tick() <= self.reloading_time() * 0.8)
	{
		double dist_tl = self.GetDistanceTo (0,					0);
		double dist_tr = self.GetDistanceTo (world.width(),		0);
		double dist_br = self.GetDistanceTo (world.width(),		world.height());
		double dist_bl = self.GetDistanceTo (0,					world.height());
		double dist_cl = self.GetDistanceTo (0,					world.height() / 2);
		double dist_cr = self.GetDistanceTo (world.width(),		world.height() / 2);
		double min_dist = min (min (min (dist_tl, dist_tr), min (dist_br, dist_bl)), min (dist_cl, dist_cr));
		if (min_dist == dist_tl)
			GoTo (move, self.GetAngleTo (0,					0));
		else if (min_dist == dist_tr)
			GoTo (move, self.GetAngleTo (world.width(),		0));
		else if (min_dist == dist_br)
			GoTo (move, self.GetAngleTo (world.width(),		world.height()));
		else if (min_dist == dist_bl)
			GoTo (move, self.GetAngleTo (0,					world.height()));
		else if (min_dist == dist_cl)
			GoTo (move, self.GetAngleTo (0,					world.height() / 2));
		else if (min_dist == dist_cr)
			GoTo (move, self.GetAngleTo (world.width(),		world.height() / 2));
		return;
	}

	Bonus *goal = NULL;
	double measure_goal = 0;
	double measure_health = MeasureSelf_Health(self, world);
	double measure_shield = MeasureSelf_Shield(self, world);
	double measure_ammo = MeasureSelf_Ammo(self, world);
	double max_bonus_dist_health = 0;
	double max_bonus_dist_shield = 0;
	double max_bonus_dist_ammo = 0;
	for (size_t i = 0; i < bonuses.size(); ++i)
	{
		double dist = bonuses[i].GetDistanceTo (self);
		switch (bonuses[i].type())
		{
		case MEDIKIT:
			{
				
				if (dist > max_bonus_dist_health)
					max_bonus_dist_health = dist;
				break;
			}
		case REPAIR_KIT:
			{
				if (dist > max_bonus_dist_shield)
					max_bonus_dist_shield = dist;
				break;
			}
		case AMMO_CRATE:
			{
				if (dist > max_bonus_dist_ammo)
					max_bonus_dist_ammo = dist;
				break;
			}
		default:
			{
				dist = 0;
				break;
			}
		}
	}
	for (size_t i = 0; i < bonuses.size(); ++i)
	{
		double measure = 0;
		switch (bonuses[i].type())
		{
		case MEDIKIT:
			{
				if (max_bonus_dist_health)
					measure = measure_health * (1 - pow (0.8 * bonuses[i].GetDistanceTo (self) / max_bonus_dist_health, 2.3));
				break;
			}
		case REPAIR_KIT:
			{
				if (max_bonus_dist_shield)
					measure = measure_shield * (1 - pow (0.8 * bonuses[i].GetDistanceTo (self) / max_bonus_dist_shield, 1.5));
				break;
			}
		case AMMO_CRATE:
			{
				if (max_bonus_dist_ammo)
					measure = measure_ammo * (1 - pow (0.8 * bonuses[i].GetDistanceTo (self) / max_bonus_dist_ammo, 0.4));
				break;
			}
		default:
			{
				measure = 0;
				break;
			}
		}
		if (measure > measure_goal)
		{
			goal = &bonuses[i];
			measure_goal = measure;
		}
	}
	if (goal && measure_health >= 0.5 || measure_shield >= 0.4)
	{
		GoTo (move, self.GetAngleTo (*goal));
		return;
	}

	if (goal)
	{
		Shell *danger = NULL;
		double danger_measure = 0;
		double max_dist_shells = 0;
		double max_angle_shells = 0;
		for (size_t i = 0; i < shells.size(); ++i)
		{
			double dist = shells[i].GetDistanceTo(self);
			double angle = fabs(shells[i].GetAngleTo (self));
			if (angle <= M_PI/2 && angle > max_angle_shells)
				max_angle_shells = angle;
			if (dist > max_dist_shells)
				max_dist_shells = dist;
		}
		if (max_angle_shells && max_dist_shells)
		{
			for (size_t i = 0; i < shells.size(); ++i)
			{
				double measure = 0;
				//MeasureShell_Defend (&self, &world, &shells[i]);
				double dist = shells[i].GetDistanceTo(self);
				double angle = fabs(shells[i].GetAngleTo (self));
				if (angle >= M_PI/2)
					measure = 0;
				else if (dist * sin(angle) <= self.width()/2)
					measure = 1;
				else if (dist * sin(angle) <= self.height())
					measure = 0.9;
				else
					measure = 0.9 * (1 - pow (0.8 * dist / max_dist_shells, 0.9)) * (1 - pow (0.8 * angle / max_angle_shells, 0.4));
				if (measure > danger_measure)
				{
					danger = &shells[i];
					danger_measure = measure;
				}
			}
		}
		if (danger && danger_measure >= MIN_DANGER_LEVEL)
			GoFrom (move, self.GetAngleTo (*danger), self.GetAngleTo (*goal));
		else
			GoTo (move, self.GetAngleTo (*goal));
	}
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}