#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

using namespace model;
using namespace std;

int CurrentEnemy = 0;
std::string enemy_name = "";
long long enemy_id = -1;
double max_dist_alive = 1E20;

const double MIN_ANGLE = M_PI / 6.0; 
const double MAX_ANGLE = M_PI; 
const double MIN_ANGLE_SO_GO = M_PI / 18.0;
const double MIN_FIRING_ANGLE = M_PI / 180.0; 
const double HELP_FIRING_ANGLE = M_PI / 90.0; 
const double MIN_FLEE_ANGLE = M_PI / 8.0;
const double ANGLE_POWER = 1.0;
const double TARGETING_TIME = 0.1;
const double EMERGENCY_MEASURE_MIN = 50;

//Returns a random number in the range (0.0f, 1.0f).
// 0111 1111 1111 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000
// seee eeee eeee vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv
// sign     = 's'
// exponent = 'e'
// value    = 'v'
double DoubleRand() {
  typedef unsigned long long uint64;
  uint64 ret=0;
  for(int i=0;i<13;i++)ret|=((uint64)(rand()%16)<<i*4);
  if(ret==0)return(rand()%2?1.0f:0.0f);
  uint64 retb=ret;
  unsigned int exp=0x3ff;
  retb=ret|((uint64)(exp)<<52);
  double *tmp=(double*)&retb;
  double retval=*tmp;
  while(retval>1.0f||retval<0.0f)
    retval=*(tmp=(double*)&(retb=ret|((uint64)(exp--)<<52)));
  if(rand()%2)retval-=0.5f;
  return retval;
}

bool Flee (Unit unit, model::Move& move)
{

}

enum MEASURES {ENEMY_DIST_FIRE, ENEMY_DIST_DEFEND, HEALTH_INNEED, HULL_INNEED, AMMO_INNEED, I_AM_A_TARGET}; 

double MeasureIt (MEASURES measure, Tank *self, World *world, Tank *enemy = NULL, Shell *shell = NULL)
{
	if (!self || !world)
		return 0;
	switch (measure)
	{
	case ENEMY_DIST_FIRE:
		{
			if (!enemy)
				return 0;
			double turret_angle = self->GetTurretAngleTo (*enemy);
			if (turret_angle == 0)
				return 1E20;
			if (max_dist_alive == 0)
				return 0;
			return ANGLE_POWER / abs (turret_angle) * self->GetDistanceTo (*enemy) / max_dist_alive;
			break;
		}
	case ENEMY_DIST_DEFEND:
		{
			if (!enemy)
				return 0;
			double turret_angle = enemy->GetTurretAngleTo (*self);
			if (turret_angle == 0)
				return 1E20;
			if (max_dist_alive == 0)
				return 0;
			if (enemy->remaining_reloading_time() == 0)
				return ANGLE_POWER / abs (turret_angle) * enemy->GetDistanceTo (*self) / max_dist_alive;
			return ANGLE_POWER / abs (turret_angle) * enemy->GetDistanceTo (*self) / max_dist_alive * enemy->reloading_time() / enemy->remaining_reloading_time();
			break;
		}
	case HEALTH_INNEED:
		{
			int hp = self->crew_health();
			if (hp == 0)
				return 0;		//я мертв
			int max_hp = self->crew_max_health();
			double hp_proc = double (hp) / max_hp;
			if (hp_proc <= 0.7)
			{
				return - (hp_proc - 0.7) / (0.7 - 0.3);
			}
			else
			{
				return 0;
			}
			break;
		}
	
	case HULL_INNEED:
		{
			int hull = self->hull_durability();
			if (hull == 0)
				return 0;		//я мертв
			int max_hull = self->crew_max_health();
			double hull_proc = double (hull) / max_hull;
			if (hull_proc <= 0.7)
			{
				return  - (hull_proc - 0.7) / (0.7 - 0.3);
			}
			else
			{
				return 0;
			}
			break;
		}
	case AMMO_INNEED:
		{
			int ammo_count = self->premium_shell_count();
			if (ammo_count)
				return 1 / 5 * ammo_count;
			else
				return 1;
			break;
		}
	case I_AM_A_TARGET:
		{
			if (!shell)
				return 0;
			double counter = 1;
			if (shell->type() == PREMIUM)
				counter = 2;
			if (!(shell->GetAngleTo (*self)) && shell->GetDistanceTo(*self))
				return 1E20;
			if (!shell->GetDistanceTo(*self))
				return 0;
			return counter / abs (shell->GetAngleTo (*self)) / shell->GetDistanceTo (*self) * (world->width());
		}
	default:				
		{
			return 0;
		}
	}
}

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	bool fire_on = false;
	bool turret_move_on = false;
	bool move_on = false;
	vector <Tank> all_tanks = world.tanks();
	vector <Shell> shells = world.shells();
	vector <Bonus> bonuses = world.bonuses();
	//pacifist mode (нет нужды стрелять, если враг не на прицеле))
	move.set_fire_type(NONE);
	vector <int> EnemiesToAttack;
	max_dist_alive = 1E20;
	//заполнение массива живых врагов. + вычисление max_dist_alive
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health()) && (all_tanks[i].hull_durability()) && (!all_tanks[i].teammate()))
			EnemiesToAttack.push_back (i);
	}
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		double dist = self.GetDistanceTo (all_tanks[i]);
		if (dist && dist < max_dist_alive && all_tanks[i].crew_health() && all_tanks[i].hull_durability())	//если ближний и живой
			max_dist_alive = dist;
	}
	Unit *closest_heal = NULL;
	Unit *closest_armor = NULL;;
	Unit *closest_turtoise = NULL;
	for (size_t i = 0; i < bonuses.size(); ++i)
	{
		switch (bonuses[i].type())
		{
		case MEDIKIT:
			{
				double dist = self.GetDistanceTo (bonuses[i]);
				if (closest_heal && closest_heal->GetDistanceTo (self) > dist)
				{
					closest_heal = &bonuses[i];
				}
				if (!closest_heal)
					closest_heal = &bonuses[i];
				break;
			}
		case AMMO_CRATE:
			{
				double dist = self.GetDistanceTo (bonuses[i]);
				if (closest_armor && closest_armor->GetDistanceTo (self) > dist)
				{
					closest_armor = &bonuses[i];
				}
				if (!closest_armor)
					closest_armor = &bonuses[i];
				break;
			}
		case REPAIR_KIT:
			{
				double dist = self.GetDistanceTo (bonuses[i]);
				if (closest_turtoise && closest_turtoise->GetDistanceTo (self) > dist)
				{
					closest_turtoise = &bonuses[i];
				}
				if (!closest_turtoise)
					closest_turtoise = &bonuses[i];
				break;
			}
		default:
			break;
		}
	}

	double Measure_health = MeasureIt (HEALTH_INNEED, &self, &world);
	double Measure_hull   = MeasureIt (HULL_INNEED, &self, &world);
	double Measure_ammo   = MeasureIt (AMMO_INNEED, &self, &world);

	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////FIRING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	Tank *enemy = NULL;
	double measure_enemy = 0;
	for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
	{
		double measure = MeasureIt (ENEMY_DIST_FIRE, &self, &world, &(all_tanks[EnemiesToAttack[i]]));
		{
			if (measure > measure_enemy)
			{
				enemy = &(all_tanks[EnemiesToAttack[i]]);
				measure_enemy = measure;
			}
		}
	}
	if (enemy)
	{
		double angle = self.GetTurretAngleTo (*enemy);
		if (abs (angle) < MIN_FIRING_ANGLE)
		{
			if (self.remaining_reloading_time() == 0)
			{
				move.set_fire_type (PREMIUM_PREFERRED);
				fire_on = true;
			}
		}
		else if (angle > 0)
		{
			move.set_turret_turn (self.turret_turn_speed());
		}
		else
		{
			move.set_turret_turn (-self.turret_turn_speed());
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////MOVING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////

	if (!fire_on)
	{
		if (Measure_health > 1 || Measure_hull > 1)
		{	//пора за жизнями или бронёй, уворачиваясь
			Bonus *goal = NULL;
			double goal_dist = 1E20;
			for (size_t i = 0; i < bonuses.size(); ++i)
			{
				double dist = bonuses[i].GetDistanceTo (self);
				if (dist < goal_dist)
				{
					if (Measure_health > 1)
					{
						if (bonuses[i].type() == MEDIKIT)
						{
							goal = &(bonuses[i]);
							goal_dist = dist;
							continue;
						}
					}
					if (Measure_hull > 1)
					{
						if (bonuses[i].type() == REPAIR_KIT)
						{
							goal = &(bonuses[i]);
							goal_dist = dist;
							continue;
						}
					}
				}
				if (!goal)
				{
					goal = &(bonuses[i]);
					goal_dist = dist;
				}
			}
			if (goal)
			{	//наклёвывается движение к бонусу, если я не рядом со снарядом
				Shell *shell = NULL;
				double shell_measure = 0;
				for (size_t i = 0; i < shells.size(); ++i)
				{
					double measure = MeasureIt (I_AM_A_TARGET, &self, &world, NULL, &(shells[i]));
					if (measure > shell_measure)
					{
						shell_measure = measure;
						shell = &(shells[i]);
					}
					if (!shell)
					{
						shell_measure = measure;
						shell = &(shells[i]);
					}
				}
				if (shell && shell_measure > EMERGENCY_MEASURE_MIN)
				{
					double angle = self.GetAngleTo (*shell);
					if (abs (angle) > MIN_FLEE_ANGLE)
					{	//пора бежать назад с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-0.5);
						}
						else
						{
							move.set_left_track_power (-0.5);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
					else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
					{	//пора бежать вперёд с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(0.5);
						}
						else
						{
							move.set_left_track_power (0.5);
							move.set_right_track_power(1.0);
						}
						move_on = true;
					}
					else
					{	//пора бежать вперёд или назад
						if (abs (angle) < M_PI / 2)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
						else
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
				}
				else
				{
					double angle = self.GetAngleTo (*goal);
					if (M_PI - MIN_ANGLE_SO_GO >= abs (angle) >= MIN_ANGLE_SO_GO)
					{
						if (angle >= 0)
						{	//направо
							if (angle <= M_PI / 2)
							{	//надо ехать вперёд и направо
								move.set_left_track_power (1.0);
								move.set_right_track_power(0.5);
							}
							else
							{	//надо ехать назад и направо
								move.set_left_track_power (-0.5);
								move.set_right_track_power(-1.0);
							}
						}
						else
						{	//налево
							if (abs (angle) <= M_PI / 2)
							{	//надо ехать вперёд и налево
								move.set_left_track_power (0.5);
								move.set_right_track_power(1.0);
							}
							else
							{	//надо ехать назад и налево
								move.set_left_track_power (-1.0);
								move.set_right_track_power(-0.5);
							}
						}
					}
					else
					{
						if (abs (angle) >= M_PI / 2)
						{	//просто назад
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						else
						{	//просто вперёд
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
					}
					move_on = true;
				}
			}
		}
		else if (Measure_health || Measure_hull || Measure_ammo)	//выполняется всегда, т.к. Measure_ammo != 0
		{	//пора за жизнями или бронёй, уворачиваясь
			Bonus *goal = NULL;
			double goal_dist = 1E20;
			for (size_t i = 0; i < bonuses.size(); ++i)
			{
				double measure_effect = 0;
				switch (bonuses[i].type())
				{
				case MEDIKIT:
					measure_effect = Measure_health;
					break;
				case REPAIR_KIT:
					measure_effect = Measure_hull;
					break;
				case AMMO_CRATE:
					measure_effect = Measure_ammo;
					break;
				default:
					measure_effect = 0;
				}
				double dist = measure_effect * bonuses[i].GetDistanceTo (self);
				if (dist < goal_dist)
				{
					if (bonuses[i].type() == MEDIKIT)
					{
						goal = &(bonuses[i]);
						goal_dist = dist;
						continue;
					}
				}
				if (!goal)
				{
					goal = &(bonuses[i]);
					goal_dist = dist;
				}
			}
			if (goal)
			{	//наклёвывается движение к бонусу, если я не рядом со снарядом
				Shell *shell = NULL;
				double shell_measure = 0;
				for (size_t i = 0; i < shells.size(); ++i)
				{
					double measure = MeasureIt (I_AM_A_TARGET, &self, &world, NULL, &(shells[i]));
					if (measure > shell_measure)
					{
						shell_measure = measure;
						shell = &(shells[i]);
					}
					if (!shell)
					{
						shell_measure = measure;
						shell = &(shells[i]);
					}
				}
				if (shell && shell_measure > EMERGENCY_MEASURE_MIN)
				{
					double angle = self.GetAngleTo (*shell);
					if (abs (angle) > MIN_FLEE_ANGLE)
					{	//пора бежать назад с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-0.5);
						}
						else
						{
							move.set_left_track_power (-0.5);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
					else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
					{	//пора бежать вперёд с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(0.5);
						}
						else
						{
							move.set_left_track_power (0.5);
							move.set_right_track_power(1.0);
						}
						move_on = true;
					}
					else
					{	//пора бежать вперёд или назад
						if (abs (angle) < M_PI / 2)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
						else
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
				}
				else
				{
					double angle = self.GetAngleTo (*goal);
					if (M_PI - MIN_ANGLE_SO_GO >= abs (angle) >= MIN_ANGLE_SO_GO)
					{
						if (angle >= 0)
						{	//направо
							if (angle <= M_PI / 2)
							{	//надо ехать вперёд и направо
								move.set_left_track_power (1.0);
								move.set_right_track_power(0.5);
							}
							else
							{	//надо ехать назад и направо
								move.set_left_track_power (-0.5);
								move.set_right_track_power(-1.0);
							}
						}
						else
						{	//налево
							if (abs (angle) <= M_PI / 2)
							{	//надо ехать вперёд и налево
								move.set_left_track_power (0.5);
								move.set_right_track_power(1.0);
							}
							else
							{	//надо ехать назад и налево
								move.set_left_track_power (-1.0);
								move.set_right_track_power(-0.5);
							}
						}
					}
					else
					{
						if (abs (angle) >= M_PI / 2)
						{	//просто назад
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						else
						{	//просто вперёд
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
					}
					move_on = true;
				}
			}
		}
	}

	if (!move_on && !fire_on)
	{
		Tank *danger = NULL;
		double danger_measure = 0;
		for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
		{
			double warning = MeasureIt (ENEMY_DIST_DEFEND, &self, &world, &(all_tanks[EnemiesToAttack[i]]));
			if (warning > danger_measure || !danger)
			{
				danger_measure = warning;
				danger = &(all_tanks[EnemiesToAttack[i]]);
			}
		}
		if (danger)
		{
			double angle = self.GetAngleTo (*danger);
			if (abs (angle) > MIN_FLEE_ANGLE)
			{	//пора бежать назад с поворотом
				if (angle > 0)
				{
					move.set_left_track_power (-1.0);
					move.set_right_track_power(-0.5);
				}
				else
				{
					move.set_left_track_power (-0.5);
					move.set_right_track_power(-1.0);
				}
				move_on = true;
			}
			else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
			{	//пора бежать вперёд с поворотом
				if (angle > 0)
				{
					move.set_left_track_power (1.0);
					move.set_right_track_power(0.5);
				}
				else
				{
					move.set_left_track_power (0.5);
					move.set_right_track_power(1.0);
				}
				move_on = true;
			}
			else
			{	//пора бежать вперёд или назад
				if (abs (angle) < M_PI / 2)
				{
					move.set_left_track_power (1.0);
					move.set_right_track_power(1.0);
				}
				else
				{
					move.set_left_track_power (-1.0);
					move.set_right_track_power(-1.0);
				}
				move_on = true;
			}
		}
	}
	return;
	//////////////////////////////////////////////////////////LOGIC///////////////////////////////////////////////
	if (Measure_health > 1 || Measure_hull > 1)
	{			//пора спасаться: увороты и беготня за аптечкой как приоритет
		if (self.turret_max_relative_angle())
		{
			//противотанковое орудие, надо спасаться как-то хитро
			if (closest_heal)
			{
				double my_tank_angle_to = self.GetAngleTo (*closest_heal);
				if (abs (my_tank_angle_to) > MIN_ANGLE)
				{
					if (my_tank_angle_to > 0)
					{
						move.set_left_track_power (1.0);
						move.set_right_track_power(-1.0);
					}
					else
					{
						move.set_left_track_power (-1.0);
						move.set_right_track_power(1.0);
					}
				}
				else
				{
					move.set_left_track_power (1.0);
					move.set_right_track_power(1.0);
				}
				move_on = true;
			}
		}
		else
		{	//можно стараться целиться и ехать за аптечкой, уворачиваясь
			if (closest_heal)
			{
				double my_tank_angle_to = self.GetAngleTo (*closest_heal);
				if (abs (my_tank_angle_to) > MIN_ANGLE)
				{
					if (my_tank_angle_to > 0)
					{
						move.set_left_track_power (1.0);
						move.set_right_track_power(-1.0);
					}
					else
					{
						move.set_left_track_power (-1.0);
						move.set_right_track_power(1.0);
					}
				}
				else
				{
					move.set_left_track_power (1.0);
					move.set_right_track_power(1.0);
				}
				move_on = true;;
			}
			Tank *closest_enemy = NULL;
			double closest_enemy_measure = 1E20;
			for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
			{
				double measure = MeasureIt (ENEMY_DIST_FIRE, &self, &world, &(all_tanks[i]));
				if (measure < closest_enemy_measure)
				{
					closest_enemy = &(all_tanks[i]);
					closest_enemy_measure = measure;
				}

			}
			if (closest_enemy)
			{
				double my_tank_angle_to = self.GetTurretAngleTo (*closest_enemy);
				if (abs (my_tank_angle_to) > MIN_FIRING_ANGLE)
				{
					if (my_tank_angle_to > 0)
					{
						move.set_turret_turn (self.turret_turn_speed());
					}
					else
					{
						move.set_turret_turn ( - self.turret_turn_speed());
					}
				}
				else
				{
					move.set_fire_type (PREMIUM_PREFERRED);
				}
				turret_move_on = true;
			}
		}
	}
	else
	{	//можно стараться повоевать
		double reload = self.remaining_reloading_time();
		double reload_max = self.reloading_time ();
		if (!reload)
		{	//готов стрелять, пора целиться
			Tank *closest_enemy = NULL;
			double closest_enemy_measure = 1E20;
			for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
			{
				double measure = MeasureIt (ENEMY_DIST_FIRE, &self, &world, &(all_tanks[i]));
				if (measure < closest_enemy_measure)
				{
					closest_enemy = &(all_tanks[i]);
					closest_enemy_measure = measure;
				}

			}
			if (closest_enemy)
			{
				double my_tank_angle_to = self.GetTurretAngleTo (*closest_enemy);
				if (abs (my_tank_angle_to) > MIN_FIRING_ANGLE)
				{
					if (my_tank_angle_to > 0)
					{
						move.set_turret_turn (self.turret_turn_speed());
						turret_move_on = true;
					}
					else
					{
						move.set_turret_turn ( - self.turret_turn_speed());
						turret_move_on = true;
					}
					if (abs (my_tank_angle_to) > HELP_FIRING_ANGLE)
					{	//помогу целиться и корпусом
						if (my_tank_angle_to > 0)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(-1.0);
						}
						else
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(1.0);
						}
						move_on = true;
					}
				}
				else
				{
					move.set_fire_type (PREMIUM_PREFERRED);
					fire_on = true;
				}
			}
		}
		else if (reload / reload_max <= TARGETING_TIME)
		{	//пора целиться, если мне ничего не угрожает
			Shell *closest_shell = NULL;
			double closest_shell_measure = 1E20;
			for (size_t i = 0; i < shells.size(); ++i)
			{
				double measure = MeasureIt (I_AM_A_TARGET, &self, &world, NULL, &(shells[i]));
				if (measure < closest_shell_measure)
				{
					closest_shell_measure = measure;
					closest_shell = &(shells[i]);
				}
			}
			if (closest_shell)
			{
				if (closest_shell->GetAngleTo (self) > 5*MIN_FIRING_ANGLE && closest_shell->GetDistanceTo (self) > 3 * self.height())
				{	//угрозы нет
					Tank *closest_enemy = NULL;
					double closest_enemy_measure = 1E20;
					for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
					{
						double measure = MeasureIt (ENEMY_DIST_FIRE, &self, &world, &(all_tanks[i]));
						if (measure < closest_enemy_measure)
						{
							closest_enemy = &(all_tanks[i]);
							closest_enemy_measure = measure;
						}
					}
					if (closest_enemy)
					{
						double my_tank_angle_to = self.GetTurretAngleTo (*closest_enemy);
						if (abs (my_tank_angle_to) > MIN_FIRING_ANGLE)
						{
							if (my_tank_angle_to > 0)
							{
								move.set_turret_turn (self.turret_turn_speed());
								turret_move_on = true;
							}
							else
							{
								move.set_turret_turn ( - self.turret_turn_speed());
								turret_move_on = true;
							}
							if (abs (my_tank_angle_to) > HELP_FIRING_ANGLE)
							{	//помогу целиться и корпусом
								if (my_tank_angle_to > 0)
								{
									move.set_left_track_power (1.0);
									move.set_right_track_power(-1.0);
								}
								else
								{
									move.set_left_track_power (-1.0);
									move.set_right_track_power(1.0);
								}
								move_on = true;
							}
						}
						else
						{
							move.set_fire_type (PREMIUM_PREFERRED);
							fire_on = true;
						}
					}
				}
				else
				{	//мне угрожает выстрел closest_shell
					double angle = self.GetAngleTo (*closest_shell);
					if (abs (angle) > MIN_FLEE_ANGLE)
					{	//пора бежать назад с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-0.5);
						}
						else
						{
							move.set_left_track_power (-0.5);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
					else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
					{	//пора бежать вперёд с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(0.5);
						}
						else
						{
							move.set_left_track_power (0.5);
							move.set_right_track_power(1.0);
						}
						move_on = true;
					}
					else
					{	//пора бежать вперёд или назад
						if (abs (angle) < M_PI / 2)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
						else
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
				}
			}
		}
		else
		{	//пока делать нечего))

		}
	}

	if (!move_on && !fire_on)
	{	//если не стреляю и не задал движение своего танка, то попробую увернуться от снаряда (если такой есть) или сбить прицел врагу
		Shell *closest_shell = NULL;
		double closest_shell_measure = 1E20;
		for (size_t i = 0; i < shells.size(); ++i)
		{
			double measure = MeasureIt (I_AM_A_TARGET, &self, &world, NULL, &(shells[i]));
			if (measure < closest_shell_measure)
			{
				closest_shell_measure = measure;
				closest_shell = &(shells[i]);
			}
		}
		if (closest_shell)
		{
			if (closest_shell->GetAngleTo (self) > 5*MIN_FIRING_ANGLE && closest_shell->GetDistanceTo (self) > 3 * self.height())
			{	//угрозы нет
				//попробую сбить врагу прицел
				Tank *closest_enemy = NULL;
				double closest_enemy_measure = 1E20;
				for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
				{
					double measure = MeasureIt (ENEMY_DIST_DEFEND, &self, &world, &(all_tanks[i]));
					if (measure < closest_enemy_measure)
					{
						closest_enemy = &(all_tanks[i]);
						closest_enemy_measure = measure;
					}
				}
				if (closest_enemy)
				{
					double angle = self.GetAngleTo (*closest_enemy);
					if (abs (angle) > MIN_FLEE_ANGLE)
					{	//пора бежать назад с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-0.5);
						}
						else
						{
							move.set_left_track_power (-0.5);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
					else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
					{	//пора бежать вперёд с поворотом
						if (angle > 0)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(0.5);
						}
						else
						{
							move.set_left_track_power (0.5);
							move.set_right_track_power(1.0);
						}
						move_on = true;
					}
					else
					{	//пора бежать вперёд или назад
						if (abs (angle) < M_PI / 2)
						{
							move.set_left_track_power (1.0);
							move.set_right_track_power(1.0);
						}
						else
						{
							move.set_left_track_power (-1.0);
							move.set_right_track_power(-1.0);
						}
						move_on = true;
					}
				}
				else
				{
					if (shells.size())
					{
						double measure = max (Measure_health, max (Measure_hull, Measure_ammo));
						Shell *target = NULL;
						for (size_t i = 0; i < shells.size(); ++i)
						{
							switch (shells[i].type())
							{
							case MEDIKIT:
								if (MeasureIt (AMMO_INNEED, &self, &world, NULL, &(shells[i])) == measure)
								{
									target = &(shells[i]);
									break;
								}
							case REPAIR_KIT:
								if (MeasureIt (HULL_INNEED, &self, &world, NULL, &(shells[i])) == measure)
								{
									target = &(shells[i]);
									break;
								}
							case AMMO_CRATE:
								if (MeasureIt (AMMO_INNEED, &self, &world, NULL, &(shells[i])) == measure)
								{
									target = &(shells[i]);
									break;
								}
							default:
								break;
							}
							if (target)
								break;
						}
						if (target)
						{
							double my_tank_angle_to = self.GetAngleTo (*target);
							if (abs (my_tank_angle_to) > MIN_ANGLE)
							{
								if (my_tank_angle_to > 0)
								{
									move.set_left_track_power (1.0);
									move.set_right_track_power(-1.0);
								}
								else
								{
									move.set_left_track_power (-1.0);
									move.set_right_track_power(1.0);
								}
							}
							else
							{
								move.set_left_track_power (1.0);
								move.set_right_track_power(1.0);
							}
							move_on = true;
						}
					}
				}
			}
			else
			{	//мне угрожает выстрел closest_shell
				double angle = self.GetAngleTo (*closest_shell);
				if (abs (angle) > MIN_FLEE_ANGLE)
				{	//пора бежать назад с поворотом
					if (angle > 0)
					{
						move.set_left_track_power (-1.0);
						move.set_right_track_power(-0.5);
					}
					else
					{
						move.set_left_track_power (-0.5);
						move.set_right_track_power(-1.0);
					}
					move_on = true;
				}
				else if (abs (angle) > M_PI - MIN_FLEE_ANGLE)
				{	//пора бежать вперёд с поворотом
					if (angle > 0)
					{
						move.set_left_track_power (1.0);
						move.set_right_track_power(0.5);
					}
					else
					{
						move.set_left_track_power (0.5);
						move.set_right_track_power(1.0);
					}
					move_on = true;
				}
				else
				{	//пора бежать вперёд или назад
					if (abs (angle) < M_PI / 2)
					{
						move.set_left_track_power (1.0);
						move.set_right_track_power(1.0);
					}
					else
					{
						move.set_left_track_power (-1.0);
						move.set_right_track_power(-1.0);
					}
					move_on = true;
				}
			}
		}
	}

	if (!turret_move_on && !fire_on)
	{
		Tank *closest_enemy = NULL;
		double closest_enemy_measure = 1E20;
		for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
		{
			double measure = MeasureIt (ENEMY_DIST_FIRE, &self, &world, &(all_tanks[i]));
			if (measure < closest_enemy_measure)
			{
				closest_enemy = &(all_tanks[i]);
				closest_enemy_measure = measure;
			}
		}
		if (closest_enemy)
		{
			double my_tank_angle_to = self.GetTurretAngleTo (*closest_enemy);
			if (abs (my_tank_angle_to) > MIN_FIRING_ANGLE)
			{
				if (my_tank_angle_to > 0)
				{
					move.set_turret_turn (self.turret_turn_speed());
					turret_move_on = true;
				}
				else
				{
					move.set_turret_turn ( - self.turret_turn_speed());
					turret_move_on = true;
				}
				if (abs (my_tank_angle_to) > HELP_FIRING_ANGLE)
				{	//помогу целиться и корпусом
					if (my_tank_angle_to > 0)
					{
						move.set_left_track_power (1.0);
						move.set_right_track_power(-1.0);
					}
					else
					{
						move.set_left_track_power (-1.0);
						move.set_right_track_power(1.0);
					}
					move_on = true;
				}
			}
			else
			{
				move.set_fire_type (PREMIUM_PREFERRED);
				fire_on = true;
			}
		}
	}

	if (move_on || turret_move_on || fire_on)
		return;

	if (CurrentEnemy >= 0 && !all_tanks[CurrentEnemy].crew_health())
		CurrentEnemy = -1;
	double dist_to_currentenemy = 1E20;
	if (CurrentEnemy >= 0)
		dist_to_currentenemy = self.GetDistanceTo (all_tanks[CurrentEnemy]);
	for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
	{
		if (CurrentEnemy < 0)
		{
			CurrentEnemy = EnemiesToAttack[i];
			dist_to_currentenemy = self.GetDistanceTo (all_tanks[CurrentEnemy]);
		}
		else
		{
			double dist = self.GetDistanceTo (all_tanks[EnemiesToAttack[i]]);
			if (dist < dist_to_currentenemy)
			{
				CurrentEnemy = EnemiesToAttack[i];
				dist_to_currentenemy = dist;
			}

		}
	}

	//Буду пытаться шматять по противнику
	if (CurrentEnemy >= 0)
	{
		Tank aim = all_tanks[CurrentEnemy];
		enemy_name = aim.player_name();
		enemy_id = aim.id();
		double turret_angle_to = self.GetTurretAngleTo (aim);
		if (abs (turret_angle_to) >= MIN_FIRING_ANGLE)
		{	//не буду шмалять, буду крутить пушку
			double my_tank_angle_to = self.GetAngleTo (aim);
			if (my_tank_angle_to > 0)
			{
				move.set_left_track_power (1.0);
				move.set_right_track_power(-1.0);
			}
			else
			{
				move.set_left_track_power (-1.0);
				move.set_right_track_power(1.0);
			}
			if (turret_angle_to > 0)
				move.set_turret_turn (self.turret_turn_speed());
			else
				move.set_turret_turn (-self.turret_turn_speed());
		}
		else
		{	//постараюсь шмалять
			if (turret_angle_to > 0)
				move.set_turret_turn (self.turret_turn_speed());
			else
				move.set_turret_turn (-self.turret_turn_speed());
			if (!self.remaining_reloading_time())
			{
				move.set_fire_type(PREMIUM_PREFERRED);
			}
		}
	}
	/*
	vector <int> shells_to_me;
	double min_dist_to_shell = 1E20;
	int closest_shell = -1;
	double tan_closest = 0;
	for (size_t i = 0; i < PreviousData_Shells.size(); ++i)
	{	
		size_t units_count = PreviousData_Shells[i].units.size();
		double tan = 0;
		if (units_count >= 2)
		{
			Unit current = PreviousData_Shells[i].units[units_count - 1];
			Unit last = PreviousData_Shells[i].units[units_count - 1];
			if (is_aim (last, current, self))
				tan = line_tan (last, current);
		}
		double dist = self.GetDistanceTo (PreviousData_Shells[i].units[units_count - 1]);
		if (dist < min_dist_to_shell)
		{
			closest_shell = i;
			min_dist_to_shell = dist;
			tan_closest = tan;
		}
	}*/

	if (100 * self.remaining_reloading_time() / self.reloading_time() > 20)
	{
		if (closest_heal && (100 * self.crew_health() / self.crew_max_health() < 50))
		{
			double angle_to_bonus = self.GetAngleTo (*closest_heal);
			if (angle_to_bonus > MIN_ANGLE) 
			{         // ���� ���� ������ �������������,
				move.set_left_track_power(0.75);      // �� ����� ���������������,
				move.set_right_track_power(-1.0);        // �������� ��������������� ���� ���������.
			} 
			else if (angle_to_bonus < -MIN_ANGLE) 
			{  // ���� ���� ������ �������������,
				move.set_left_track_power(-1.0);         // ����� ���������������
				move.set_right_track_power(0.75);     // � ��������������� �������.
			} 
			else 
			{
				move.set_left_track_power(1.0);         // ���� ���� �� ������ 30 ��������
				move.set_right_track_power(1.0);        // ������ ����������� ������ ������ 
			}
		}
		else if (closest_turtoise && (100 * self.hull_durability() / self.hull_max_durability() < 30))
		{
			double angle_to_bonus = self.GetAngleTo (*closest_turtoise);
			if (angle_to_bonus > MIN_ANGLE) 
			{         // ���� ���� ������ �������������,
				move.set_left_track_power(0.75);      // �� ����� ���������������,
				move.set_right_track_power(-1.0);        // �������� ��������������� ���� ���������.
			} 
			else if (angle_to_bonus < -MIN_ANGLE) 
			{  // ���� ���� ������ �������������,
				move.set_left_track_power(-1.0);         // ����� ���������������
				move.set_right_track_power(0.75);     // � ��������������� �������.
			} 
			else 
			{
				move.set_left_track_power(1.0);         // ���� ���� �� ������ 30 ��������
				move.set_right_track_power(1.0);        // ������ ����������� ������ ������ 
			}
		}
		else if (closest_armor)
		{
			double angle_to_bonus = self.GetAngleTo (*closest_armor);
			if (angle_to_bonus > MIN_ANGLE) 
			{         // ���� ���� ������ �������������,
				move.set_left_track_power(0.75);      // �� ����� ���������������,
				move.set_right_track_power(-1.0);        // �������� ��������������� ���� ���������.
			} 
			else if (angle_to_bonus < -MIN_ANGLE) 
			{  // ���� ���� ������ �������������,
				move.set_left_track_power(-1.0);         // ����� ���������������
				move.set_right_track_power(0.75);     // � ��������������� �������.
			} 
			else 
			{
				move.set_left_track_power(1.0);         // ���� ���� �� ������ 30 ��������
				move.set_right_track_power(1.0);        // ������ ����������� ������ ������ 
			}
		}			
		else
		{		//не буду собирать бонусы, могу либо от пуль уворачиваться, либо просто кататься рандомно
			//пока просто покатаюсь рандомно
			srand ((unsigned) time (NULL));
			double power_left  = 1.0 - DoubleRand() / 3;
			double power_right = 1.0 - DoubleRand() / 3;
			if (rand ())
				power_left = -power_left;
			if (rand ())
				power_right = -power_right;

			move.set_left_track_power  (power_left);
			move.set_right_track_power (power_right);
		}
	}

	/*
	vector<Tank> all_tanks = world.tanks();             // ������� ������ ���� �������
    double min_dist_to_bonus = 1E20;
    size_t selected_bonus = all_tanks.size();
    for(size_t i = 0; i < all_tanks.size(); ++i) {         // ���������� ����� �� ������
       Tank bonus = all_tanks[i];
       double dist_to_bonus = self.GetDistanceTo(bonus);    // ������ ���������� �� ������
	   if ((dist_to_bonus > 0 ) && (bonus.crew_health() > 0) && (bonus.speed_x() || bonus.speed_y() )) {             // ������ ���������
         min_dist_to_bonus = dist_to_bonus;
         selected_bonus = i;
       }
    }

    if (selected_bonus != all_tanks.size()) {
       double angle_to_bonus = self.GetAngleTo(all_tanks[selected_bonus]); // ������ ���� �� ������

       if (angle_to_bonus > MIN_ANGLE) {         // ���� ���� ������ �������������,
         move.set_left_track_power(0.75);      // �� ����� ���������������,
         move.set_right_track_power(-1.0);        // �������� ��������������� ���� ���������.
       } else if (angle_to_bonus < -MIN_ANGLE) {  // ���� ���� ������ �������������,
         move.set_left_track_power(-1.0);         // ����� ���������������
         move.set_right_track_power(0.75);     // � ��������������� �������.
       } else {
         move.set_left_track_power(1.0);         // ���� ���� �� ������ 30 ��������
         move.set_right_track_power(1.0);        // ������ ����������� ������ ������ 
       }
    }
	    if (selected_bonus != all_tanks.size()) {
       double angle_to_bonus = self.GetTurretAngleTo(all_tanks[selected_bonus]); // ������ ���� �� ������

	   if (angle_to_bonus) {         // ���� ���� ������ �������������,
		   move.set_turret_turn (-all_tanks[my_tank_index].turret_turn_speed());
	   }
	   else
	   {
		   move.set_turret_turn (-all_tanks[my_tank_index].turret_turn_speed());
	   }
    }*/
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}