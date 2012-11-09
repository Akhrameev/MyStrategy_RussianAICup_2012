#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <iostream>

using namespace model;
using namespace std;

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
	if (ammo <= 0)
		return 0.3;
	return 0.3 / ammo;
}

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	try {
    vector <Tank> all_tanks = world.tanks();
	vector <Shell> shells = world.shells();
	vector <Bonus> bonuses = world.bonuses();
	move.set_fire_type(NONE);
	vector <Tank> EnemiesToAttack;
	//заполнение массива живых врагов.
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health() > 0) && (all_tanks[i].hull_durability() > 0) && (!all_tanks[i].teammate()) && (!(all_tanks[i].player_name() == "EmptyPlayer")) /*&& (!(all_tanks[i].player_name().substr (0,9) == "Akhrameev"))*/)
			EnemiesToAttack.push_back (all_tanks[i]);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////FIRING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	size_t tank_num_in_enemies_array = EnemiesToAttack.size();
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
			if (measure <= 0)
				continue;
			if (measure > measure_enemy)
			{
				tank_num_in_enemies_array = i;
				measure_enemy = measure;
			}
		}
	}
	if (tank_num_in_enemies_array < EnemiesToAttack.size())
	{		//определил врага
		Tank enemy_object = EnemiesToAttack[tank_num_in_enemies_array];
		if (self.remaining_reloading_time () / self.reloading_time() <= 0.7)
		{
			double angle = self.GetTurretAngleTo (enemy_object);
			double dist = self.GetDistanceTo (enemy_object);
			if (fabs (angle) <= MIN_FIRING_ANGLE)
			{
				if (self.remaining_reloading_time() == 0)
				{
					if (dist * fabs (sin (angle)) <= enemy_object.height()/2 && dist <= world.height()/2)
						move.set_fire_type (PREMIUM_PREFERRED);
					else
						move.set_fire_type (REGULAR_FIRE);
				}
			}
			else if (angle >= 0)
				move.set_turret_turn (self.turret_turn_speed());
			else
				move.set_turret_turn (-self.turret_turn_speed());
			//if ((double (self.remaining_reloading_time()) / self.reloading_time() < 0.05) && (fabs (angle) >= MIN_FIRING_ANGLE))
			//{
			//	double right = 1;
			//	if (angle < 0)
			//		right = -1;
			//	move.set_left_track_power  (right);
			//	move.set_right_track_power (-right);
			//	if (fabs (angle) <= HELP_FIRING_ANGLE)
			//		fire_on = true;
			//}
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

	size_t goal_num_in_bonuses_array = bonuses.size();
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
			goal_num_in_bonuses_array = i;
			measure_goal = measure;
		}
	}
	if (goal_num_in_bonuses_array < bonuses.size() && (measure_health >= 0.5 || measure_shield >= 0.4))
	{
		Bonus bonus_goal = bonuses[goal_num_in_bonuses_array];
		GoTo (move, self.GetAngleTo (bonus_goal));
		return;
	}

	if (goal_num_in_bonuses_array < bonuses.size())
	{
		Bonus goal_object = bonuses[goal_num_in_bonuses_array];
		double danger_measure = 0;
		double max_dist_shells = 0;
		double max_angle_shells = 0;
		vector <Shell> shells_danger;
		for (size_t i = 0; i < shells.size(); ++i)
		{
			double dist = shells[i].GetDistanceTo(self);
			double angle = fabs(shells[i].GetAngleTo (self));
			if (angle >= M_PI/2 || dist <= self.width()/2)
				continue;
			if (angle > max_angle_shells)
				max_angle_shells = angle;
			if (dist > max_dist_shells)
				max_dist_shells = dist;
			shells_danger.push_back(shells[i]);
		}
		size_t danger_num_in_danger_array = shells_danger.size();
		if (max_angle_shells && max_dist_shells)
		{
			for (size_t i = 0; i < shells_danger.size(); ++i)
			{
				double measure = 0;
				//MeasureShell_Defend (&self, &world, &shells[i]);
				double dist = shells_danger[i].GetDistanceTo(self);
				double angle = fabs(shells_danger[i].GetAngleTo (self));
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
					danger_num_in_danger_array = i;
					danger_measure = measure;
				}
			}
		}
		if (danger_num_in_danger_array < shells_danger.size() && danger_measure >= MIN_DANGER_LEVEL)
			GoFrom (move, self.GetAngleTo (shells_danger[danger_num_in_danger_array]), self.GetAngleTo (goal_object));
		else
			GoTo (move, self.GetAngleTo (goal_object));
	}
	}
	catch (out_of_range& oor) {
		cerr << "Out of Range error: " << oor.what() << endl;
	}
	catch (...)
	{
		cerr << "tratata" << endl;
	}
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}