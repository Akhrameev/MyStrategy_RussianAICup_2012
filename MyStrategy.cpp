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
const double MIN_DANGER_LEVEL = 0.6;

//Углы задаются как pair<double, double>, углы меняются от -M_PI до M_PI; лучи отсортированы по возрастанию; рассматривается наименьший угол ( <= M_PI) между лучами;

bool Intersection_Angle (pair <double, double> Coverer, pair <double, double> Grasped)
{	//улог Coverer пересекается с углом Grasper?
	if (Coverer.second - Coverer.first <= M_PI)
	{
		if (Grasped.first > Coverer.second || Grasped.second < Coverer.first)
			return false;
		else
			return true;
	}
	else
	{
		bool changed = false;
		if (Grasped.first >= 0 && Grasped.second >= 0)
		{
			Coverer.first = Coverer.second;
			Coverer.second = M_PI;
			changed = true;
		}
		else if (Grasped.first <= 0 && Grasped.second <= 0)
		{
			Coverer.second = Coverer.first;
			Coverer.first = -M_PI;
			changed = true;
		}
		if (changed)
			return Intersection_Angle (Coverer, Grasped);
		if (Grasped.first > Coverer.first || Grasped.second < Coverer.second)
			return false;
		else
			return true;
	}
}

bool Cover_Angle (pair <double, double> Coverer, pair <double, double> Grasped)
{	//угол Covered включает в себя угол Grasped?
	if (Coverer.second - Coverer.first <= M_PI)
	{
		if (Grasped.first >= Coverer.first && Grasped.second <= Coverer.second)
			return true;
		else
			return false;
	}
	else
	{
		bool changed = false;
		if (Grasped.first >= 0 && Grasped.second >= 0)
		{
			Coverer.first = Coverer.second;
			Coverer.second = M_PI;
			changed = true;
		}
		else if (Grasped.first <= 0 && Grasped.second <= 0)
		{
			Coverer.second = Coverer.first;
			Coverer.first = -M_PI;
			changed = true;
		}
		if (changed)
			return Cover_Angle (Coverer, Grasped);
		if (Grasped.first <= Coverer.first && Grasped.second >= Coverer.second)
			return true;
		else
			return false;
	}
}

pair <double, double> Boundry_Angle (Unit &self, Unit &target)
{
	double angle_target = target.angle();
	double width = target.width();
	double height = target.height();
	double angle_vertex = atan (height/width);
	double dist = sqrt (pow(height/2, 2) + pow(width/2, 2));
	//double angle = self.GetAngleTo (target);
	pair <double, double> left_front	(target.x() + dist*cos(angle_target + angle_vertex), target.y() + dist*sin(angle_target + angle_vertex));
	angle_vertex = -angle_vertex;
	pair <double, double> right_front	(target.x() + dist*cos(angle_target + angle_vertex), target.y() + dist*sin(angle_target + angle_vertex));
	angle_vertex = angle_vertex + M_PI;
	pair <double, double> left_rear		(target.x() + dist*cos(angle_target + angle_vertex), target.y() + dist*sin(angle_target + angle_vertex));
	angle_vertex = -angle_vertex;
	pair <double, double> right_rear	(target.x() + dist*cos(angle_target + angle_vertex), target.y() + dist*sin(angle_target + angle_vertex));
	double angle_left_front =	self.GetAngleTo (left_front.first, left_front.second);
	double angle_right_front =	self.GetAngleTo (right_front.first, right_front.second);
	double angle_left_rear =	self.GetAngleTo (left_rear.first, left_rear.second);
	double angle_right_rear =	self.GetAngleTo (right_rear.first, right_rear.second);
	double min_angle = min (min (angle_left_front, angle_right_front), min (angle_left_rear, angle_right_rear));
	double max_angle = max (max (angle_left_front, angle_right_front), max (angle_left_rear, angle_right_rear));
	return pair <double, double> (min_angle, max_angle);
}

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

bool GoFrom (model::Move& move, Tank &self, double angle, double angle_to_go = 0)
{
	/*double ftangs = tan (fabs (angle));
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
	}*/
	double differance_angle = fabs (angle/* - self.angle()*/);
	if (differance_angle >= M_PI)
		differance_angle = 2 * M_PI - differance_angle;
	double measure = fabs (tan (differance_angle));
	double measure_25 = fabs(tan (M_PI / 180 * 25));
	double measure_45 = fabs(tan (M_PI / 180 * 45));
	//double measure_65 = fabs(tan (M_PI / 180 * 65));
	if (measure_25 <= measure && measure <= measure_45)
	{
		if (tan (differance_angle) <= 0)
		{	//поворот направо
			move.set_left_track_power  (1.0);
			move.set_right_track_power (-1.0);
		}
		else
		{	//поворот налево
			move.set_left_track_power  (-1.0);
			move.set_right_track_power (1.0);
		}
	}
	//else if (measure_45 <= measure && measure <= measure_65)
	//{
	//
	//}
	else
		GoTo (move, angle_to_go);
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

void AvoidShells (model::Move &move, World &world, Tank &self, vector <Tank> &EnemiesToAttack, vector <Unit> &Barrier, pair <double, double> goal)
{
	vector <Shell> shells = world.shells();
	double danger_measure = 0;
	double max_dist_shells = 0;
	double max_angle_shells = 0;
	vector <Shell> shells_danger;
	for (size_t i = 0; i < shells.size(); ++i)
	{
		double dist = shells[i].GetDistanceTo(self);
		double angle = fabs(shells[i].GetAngleTo (self));
		if (angle >= M_PI/2/* || dist <= self.width()/2*/)
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
			{
				measure = 0;
				continue;
			}
			/*else if (dist * sin(angle) <= self.width()/2)
				measure = 1;
			else if (dist * sin(angle) <= self.height())
				measure = 0.9;
			else*/
			pair <double, double> MyBoundry = Boundry_Angle (shells[i], self);
			size_t barriers = 0;
			for (size_t j = 0; j < Barrier.size(); ++j)
			{
				pair <double, double> barrier_boundry = Boundry_Angle (self, Barrier[j]);
				if (Cover_Angle (barrier_boundry, MyBoundry))
				{
					//бонус или что-то другое полностью закрывает меня
					barriers += Barrier.size();
					break;
				}
				if (Intersection_Angle (barrier_boundry, MyBoundry))
					++barriers;
			}
			if (Cover_Angle (MyBoundry, pair <double, double> (shells[i].GetAngleTo(self), shells[i].GetAngleTo(self))))
				measure = 0.9 * (1 - pow (0.8 * dist / max_dist_shells, 0.9)) * (1 - pow (0.8 * angle / max_angle_shells, 0.4));
			else
				continue;
			if (barriers > 0)
				measure /= barriers;
			if (measure > danger_measure)
			{
				danger_num_in_danger_array = i;
				danger_measure = measure;
			}
		}
	}
	if (danger_num_in_danger_array < shells_danger.size() /*&& danger_measure >= MIN_DANGER_LEVEL*/)
		GoFrom (move, self, self.GetAngleTo (shells_danger[danger_num_in_danger_array]), self.GetAngleTo (goal.first, goal.second));
	else
		GoTo (move, self.GetAngleTo (goal.first, goal.second));
}

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
    vector <Tank> all_tanks = world.tanks();
	vector <Shell> shells = world.shells();
	vector <Bonus> bonuses = world.bonuses();
	move.set_fire_type(NONE);
	vector <Tank> EnemiesToAttack;
	vector <Unit> Barrier;
	//заполнение массива живых врагов.
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health() > 0) && (all_tanks[i].hull_durability() > 0) && (!all_tanks[i].teammate()) && (!(all_tanks[i].player_name() == "EmptyPlayer")) && (!(all_tanks[i].player_name().substr (0,9) == "Akhrameev")))
			EnemiesToAttack.push_back (all_tanks[i]);
		else
			if (all_tanks[i].id() != self.id())
				Barrier.push_back (all_tanks[i]);
	}
	if (EnemiesToAttack.size() == 0)
	{
		Barrier.clear();
		for (size_t i = 0; i < all_tanks.size(); ++i)
		{
			if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health() > 0) && (all_tanks[i].hull_durability() > 0) && (!all_tanks[i].teammate()))
				EnemiesToAttack.push_back (all_tanks[i]);
			else
				if (all_tanks[i].id() != self.id())
					Barrier.push_back (all_tanks[i]);
		}
	}
	for (size_t i = 0; i < bonuses.size(); ++i)
		Barrier.push_back (bonuses[i]);

	//for (size_t i = 0; i < shells.size(); ++i)
	//	for (size_t j = i + 1; j < shells.size(); ++j)
	//		if (i != j && fabs (shells[i].GetAngleTo (shells[j])) < M_PI/360 && fabs (shells[j].GetAngleTo (shells[i])) < M_PI/360)
	//			Barrier.push_back(shells[i]);

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
			pair <double, double> enemy_boundry;
			bool BoundryCounted = false;
			size_t barriers = 0;
			for (size_t j = 0; j < Barrier.size(); ++j)
			{
				double barrier_dist = Barrier[j].GetDistanceTo(self);
				double enemy_dist = enemy_obj.GetDistanceTo(self);
				if (barrier_dist < enemy_dist)
				{
					if (!BoundryCounted)
					{
						enemy_boundry = Boundry_Angle (self, enemy_obj);
						BoundryCounted = true;
					}
					pair <double, double> barrier_boundry = Boundry_Angle (self, Barrier[j]);
					if (Cover_Angle (barrier_boundry, enemy_boundry))
					{
						measure = 0;
						continue;
					}
					if (Intersection_Angle (barrier_boundry, enemy_boundry))
						++barriers;
				}
			}
			if (barriers > 0)
				measure = measure / barriers;
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
					pair <double, double> enemy_boundry = Boundry_Angle (self, enemy_object);
					if (Cover_Angle (enemy_boundry, pair <double, double> (angle, angle)))
					{
						size_t barriers = 0;
						for (size_t j = 0; j < Barrier.size(); ++j)
						{
							double barrier_dist = Barrier[j].GetDistanceTo(self);
							double enemy_dist = enemy_object.GetDistanceTo(self);
							if (barrier_dist < enemy_dist)
							{
								pair <double, double> barrier_boundry = Boundry_Angle (self, Barrier[j]);
								if (Cover_Angle (barrier_boundry, enemy_boundry))
								{
									//бонус или что-то другое полностью закрывает врага
									barriers += Barrier.size();
									break;
								}
								if (Intersection_Angle (barrier_boundry, enemy_boundry))
									++barriers;
							}
						}
						if (!barriers && dist <= world.width()/2)
							move.set_fire_type (PREMIUM_PREFERRED);
						else if (barriers < Barrier.size())
							move.set_fire_type (REGULAR_FIRE);
					}
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
		pair <double, double> goal;
		if (min_dist == dist_tl)
			goal = pair <double, double>	(0,					0);
			//GoTo (move, self.GetAngleTo (0,					0));
		else if (min_dist == dist_tr)
			goal = pair <double, double>	(world.width(),		0);
			//GoTo (move, self.GetAngleTo (world.width(),		0));
		else if (min_dist == dist_br)
			goal = pair <double, double>	(world.width(),		world.height());
			//GoTo (move, self.GetAngleTo (world.width(),		world.height()));
		else if (min_dist == dist_bl)
			goal = pair <double, double>	(0,					world.height());
			//GoTo (move, self.GetAngleTo (0,					world.height()));
		else if (min_dist == dist_cl)
			goal = pair <double, double>	(0,					world.height() / 2);
			//GoTo (move, self.GetAngleTo (0,					world.height() / 2));
		else if (min_dist == dist_cr)
			goal = pair <double, double>	(world.width(),		world.height() / 2);
			//GoTo (move, self.GetAngleTo (world.width(),		world.height() / 2));
		AvoidShells (move, world, self, EnemiesToAttack, Barrier, goal);
		return;//надо переделать, а то первый снаряд всегда в меня - нужно вставить проверку пуль
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
		double dist = self.GetDistanceTo (bonuses[i]);
		double angle = self.GetAngleTo (bonuses[i]);
		size_t closer_enemies = 0;
		for (size_t j = 0; j < EnemiesToAttack.size(); ++j)
		{
			if (dist > EnemiesToAttack[j].GetDistanceTo (bonuses[i]))
				++closer_enemies;
			if (angle > EnemiesToAttack[j].GetAngleTo (bonuses[i]) && dist > EnemiesToAttack[j].GetDistanceTo (bonuses[i]))
				++closer_enemies;
		}
		//if (closer_enemies)
			//measure /= closer_enemies;
		if (measure > measure_goal)
		{
			goal_num_in_bonuses_array = i;
			measure_goal = measure;
		}
	}

	if (goal_num_in_bonuses_array < bonuses.size())
	{
		Bonus goal_object = bonuses[goal_num_in_bonuses_array];
		pair <double, double> goal (goal_object.x(), goal_object.y());
		AvoidShells (move, world, self, EnemiesToAttack, Barrier, goal);
	}
	else
	{
		//нужна рандомная цель))
		//pair <double, double> goal (goal_object.x(), goal_object.y());
		//AvoidShells (move, world, self, EnemiesToAttack, Barrier, goal);
	}
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}

