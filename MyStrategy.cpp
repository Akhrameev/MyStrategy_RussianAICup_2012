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
const double HELP_FIRING_ANGLE = M_PI / 18.0; 
const double MIN_FLEE_ANGLE = M_PI / 8.0;
const double ANGLE_POWER = 1.0;
const double TARGETING_TIME = 0.1;
const double EMERGENCY_MEASURE_MIN = 10;

bool GoTo (model::Move& move, double angle)
{
	double right = 1;
	if (angle < 0)
		right = -1;
	if (abs (angle) <= MIN_ANGLE)
	{
		move.set_left_track_power  (1.0);
		move.set_right_track_power (1.0);
	}
	else if (abs (angle) >= M_PI - MIN_ANGLE)
	{
		move.set_left_track_power  (- 1.0);
		move.set_right_track_power (- 1.0);
	}
	else
	{
		move.set_left_track_power  (right);
		move.set_right_track_power (-right);
	}
	return true;
}
bool GoFrom (model::Move& move, double angle, double angle_to_go = 0)
{
	double right = 1;
	if (angle < 0)
		right = -1;
	double front = 1;
	if (abs (angle) >= M_PI / 3)
		front = -1;
	if (abs (angle) <= M_PI / 3 * 2)
		front = 0;
	if (right > 0 && front > 0)
	{
		move.set_left_track_power  (-1.0);
		move.set_right_track_power (0.2);
	}
	else if (right > 0 && front < 0)
	{
		move.set_left_track_power  (1.0);
		move.set_right_track_power (-0.2);
	}
	else if (right < 0 && front < 0)
	{
		move.set_left_track_power  (-0.2);
		move.set_right_track_power (1.0);
	}
	else if (right < 0 && front > 0)
	{
		move.set_left_track_power  (0.2);
		move.set_right_track_power (-1.0);
	}
	if (0 == front)
	{
		if (abs (angle_to_go) <= M_PI / 2)
			angle_to_go = 1;
		else
			angle_to_go = -1;
		move.set_left_track_power  (angle_to_go);
		move.set_right_track_power (angle_to_go);
	}
	return true;
}

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
				return 1E10;
			if (max_dist_alive == 0)
				return 0;
			if (enemy->crew_health() == 0 || enemy->hull_durability() == 0)
				return 0;
			double sick_priority = 1;
			if (enemy->crew_health() <= 20 || enemy->hull_durability() <= 20)
				sick_priority = 100;
			if (self->premium_shell_count() && (enemy->crew_health() <= 35 || enemy->hull_durability() <= 35))
				sick_priority = 1000;
			return sick_priority * ANGLE_POWER / abs (turret_angle) * self->GetDistanceTo (*enemy) / max_dist_alive * enemy->crew_max_health() / enemy->crew_health() * enemy->hull_max_durability() / enemy->hull_durability();
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
			double premium_ammo_priority = 1;
			if (enemy->premium_shell_count())
				premium_ammo_priority = enemy->premium_shell_count();
			double firing_priority = 1;
			if (enemy->remaining_reloading_time()/ enemy->reloading_time() < 0.1)
				firing_priority = 10 * premium_ammo_priority * enemy->reloading_time() / enemy->remaining_reloading_time();
			else if (enemy->remaining_reloading_time()/ enemy->reloading_time() > 0.3)
				return 0;	//стрельнёт ещё не скоро
			return firing_priority * ANGLE_POWER / abs (turret_angle) * enemy->GetDistanceTo (*self) / max_dist_alive;
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
				return - (hp_proc - 0.7) / (0.7 - 0.5);
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
			if (!shell->GetDistanceTo(*self) || abs (shell->GetAngleTo (*self)) >= M_PI / 2)
				return 0;
			if (abs (shell->GetAngleTo (*self)) < M_PI / 36)
				counter *= 3;
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
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health()) && (all_tanks[i].hull_durability()) && (!all_tanks[i].teammate()) && (!(all_tanks[i].player_name() == "EmptyPlayer")) && (!(all_tanks[i].player_name().substr (0,9) == "Akhrameev")))
			EnemiesToAttack.push_back (i);
	}
	if (EnemiesToAttack.size() == 0)
	{
		for (size_t i = 0; i < all_tanks.size(); ++i)
		{
			if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health()) && (all_tanks[i].hull_durability()) && (!all_tanks[i].teammate()) && (!(all_tanks[i].player_name().substr (0,9) == "Akhrameev")))
				EnemiesToAttack.push_back (i);
		}
	}
	if (EnemiesToAttack.size() == 0)
	{
		for (size_t i = 0; i < all_tanks.size(); ++i)
		{
			if ((all_tanks[i].id() != self.id()) && (all_tanks[i].crew_health()) && (all_tanks[i].hull_durability()) && (!all_tanks[i].teammate()))
				EnemiesToAttack.push_back (i);
		}
	}
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		double dist = self.GetDistanceTo (all_tanks[i]);
		if (dist && dist < max_dist_alive && all_tanks[i].crew_health() && all_tanks[i].hull_durability())	//если ближний и живой
			max_dist_alive = dist;
	}
	/*
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
	}*/

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
		if ((double (self.remaining_reloading_time()) / self.reloading_time() < 0.05) && (abs (angle) >= MIN_FIRING_ANGLE))
		{
			double right = 1;
			if (angle < 0)
				right = -1;
			move.set_left_track_power  (right);
			move.set_right_track_power (-right);
			if (abs (angle) <= HELP_FIRING_ANGLE)
				fire_on = true;
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
				double angle_dist = 1;
				double angle_to_bonus = self.GetAngleTo(bonuses[i]);
				if (abs (angle_to_bonus) > M_PI / 6)
					angle_dist *= 3;
				if (abs (angle_to_bonus) > M_PI / 2)
					angle_dist *= 3;
				double dist = angle_dist * bonuses[i].GetDistanceTo (self);
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
					move_on = GoTo (move, angle);
					
				}
				else
				{
					double angle = self.GetAngleTo (*goal);
					move_on = GoTo (move, angle);
				}
			}
		}
		else if (Measure_health || Measure_hull || Measure_ammo)	//выполняется всегда, т.к. Measure_ammo != 0
		{	
			Bonus *goal = NULL;
			double goal_dist = 1E20;
			for (size_t i = 0; i < bonuses.size(); ++i)
			{
				double measure_effect = 1;
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
				double angle_to_bonus = bonuses[i].GetAngleTo (self);
				if (abs (angle_to_bonus) > M_PI / 6)
				{
					measure_effect /= 3;
				}
				if (abs (angle_to_bonus) > M_PI / 2)
				{
					measure_effect /= 3;
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
			{	//наклёвывается движение к бонусу, если я не рядом со снарядом *и рядом со мной нет опасного противника
				
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
				Tank *danger = NULL;
				double danger_measure = 0;
				for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
				{
					double warning = MeasureIt (ENEMY_DIST_DEFEND, &self, &world, &all_tanks[EnemiesToAttack[i]]);
					if (warning > danger_measure || !danger)
					{
						danger = &all_tanks[EnemiesToAttack[i]];
						danger_measure = warning;
					}
				}
				if (shell && (shell_measure > EMERGENCY_MEASURE_MIN || ((shell->GetDistanceTo(self) <= 2 * self.height()) && (abs (shell->GetAngleTo(self)) < M_PI / 2))))
				{
					double angle = self.GetAngleTo (*shell);
					if (danger && danger_measure > shell_measure)
					{
						angle = self.GetAngleTo (*danger);
						move_on = GoFrom (move, angle, self.GetAngleTo (*goal));
					}
				}
				else
				{
					double angle = self.GetAngleTo (*goal);
					move_on = GoTo (move, angle);
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
			move_on = GoTo (move, angle);
		}
	}
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}