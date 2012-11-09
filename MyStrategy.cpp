#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

using namespace model;
using namespace std;

double max_dist_alive = 891;	//sqrt (1280^2 + 800^2)
double min_angle_alive = M_PI;	//>M_PI

const double MIN_ANGLE = M_PI / 6.0; 
const double MIN_GOTO_ANGLE = M_PI / 12.0;
const double MIN_FIRING_ANGLE = M_PI / 180.0;
const double MIN_DANGER_LEVEL = 0.7;

bool GoTo (model::Move& move, double angle)
{
	double ftangs = tan (fabs (angle));
	double tangs = tan (angle);
	if (ftangs <= 1/3 && ftangs >= -1/3)
	{
		if (angle >= 0)
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
	else if (tangs < 0)
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
	if (ftangs >= 2 || ftangs <= -2)
	{
		if (angle_to_go >= 0)
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
	else if (tangs > 0)
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

double short_angle (double angle, double left = 0, double right = 0.1)
{
	if (!min_angle_alive)
		return right;
	if (!angle)
		return right;
	return left + min_angle_alive / angle / (right - left);
}

double MeasureEnemy_ToAtack (Tank *self, World *world, Tank *enemy)
{
	if (!self || !world || !enemy)
		return 0;
	double dist = self->GetDistanceTo (*enemy);
	double angle = fabs (self->GetTurretAngleTo (*enemy));
	double measure = dist * sin (angle);
	double height = enemy->height();
	double width = enemy->width();
	if ((measure <= width/2) && (dist <= height))
		return 1;
	if (measure <= height/2 && dist <= 4 *height)
		return (0.8 + short_angle(angle));
	if (measure <= height && dist <= 4 *height)
		return (0.7 + short_angle(angle));
	double health_prefer = 0.5;
	if (enemy->crew_health() < 25 || enemy->hull_durability() < 25)
		health_prefer = 1;
	else
		health_prefer *= pow (double (enemy->crew_health()) / enemy->crew_max_health(), 3) * pow (double (enemy->hull_durability()) / enemy->hull_max_durability(), 2);
	return health_prefer * pow (max_dist_alive / dist / 0.7, 2) * pow (min_angle_alive / angle / 0.7, 3);
}

double MeasureShell_Defend (Tank *self, World *world, Shell *bullet)
{
	if (!self || !world || !bullet)
		return 0;
	vector <Shell> shells = world->shells();
	double max_dist_shells = 0;
	for (size_t i = 0; i < shells.size(); ++i)
	{
		double distance = shells[i].GetDistanceTo (*self);
		double atack_angle = shells[i].GetAngleTo (*self);
		double measure = distance * sin (atack_angle);
		if (measure <= 2 * self->height() || distance > max_dist_shells)
			max_dist_shells = distance;
	}
	double dist = bullet->GetDistanceTo (*self);
	double angle = fabs (bullet->GetAngleTo   (*self));
	double measure = dist * sin (angle);
	double height = self->height();
	double width = self->width();
	if (measure <= width/2 && dist <= height)
		return 1;
	if (measure <= height/2 && dist <= 4 *height)
		return (0.8 + short_angle(angle));
	if (measure <= height && dist <= 4 *height)
		return (0.7 + short_angle(angle));
	if (measure >= 2 * height)
		return 0;
	return pow (max_dist_shells / dist / 0.7, 2) * pow (min_angle_alive / angle / 0.7, 3);
}

double MeasureEnemy_Defend (Tank *self, World *world, Tank *enemy)
{
	if (!self || !world || !enemy)
		return 0;
	double dist = enemy->GetDistanceTo (*self);
	double angle = fabs (enemy->GetTurretAngleTo   (*self));
	double measure = dist * sin (angle);
	double height = self->height();
	double width = self->width();
	if (measure <= width/2 && dist <= height)
		return 1;
	if (measure <= height/2 && dist <= 4 *height)
		return (0.8 + short_angle(angle));
	if (measure <= height && dist <= 4 *height)
		return (0.7 + short_angle(angle));
	if (measure >= 2 * height)
		return 0;
	return pow (max_dist_alive / dist / 0.7, 3) * pow (min_angle_alive / angle / 0.7, 2);
}

double MeasureSelf_Health (Tank *self, World *world)
{
	if (!self || !world)
		return 0;
	double health = self->crew_health() / self->crew_max_health();
	if (health <= 0.25)
		return 1;
	if (health <= 0.7)
		return 1 - health;
	return 1 - health;	//сюда бы квадрат
}

double MeasureSelf_Shield (Tank *self, World *world)
{
	if (!self || !world)
		return 0;
	double shield = self->hull_durability() / self->hull_max_durability();
	if (self->hull_durability() <= 25)
		return 1;
	if (shield <= 0.5)
		return 1 - shield;
	return 1 - shield;	//сюда бы квадрат
}

double MeasureSelf_Ammo (Tank *self, World *world)
{
	if (!self || !world)
		return 0;
	double ammo = self->premium_shell_count();
	if (!ammo)
		return 0.3;
	return 0.3 / ammo;
}

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	vector <Tank> all_tanks = world.tanks();
	vector <Shell> shells = world.shells();
	vector <Bonus> bonuses = world.bonuses();
	move.set_fire_type(NONE);
	vector <int> EnemiesToAttack;
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
	max_dist_alive = 0;
	min_angle_alive = 10;
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		double dist = self.GetDistanceTo (all_tanks[i]);
		if (dist && dist > max_dist_alive && all_tanks[i].crew_health() && all_tanks[i].hull_durability())	//если ближний и живой
			max_dist_alive = dist;
		double angle = fabs (self.GetAngleTo (all_tanks[i]));
		if (angle < min_angle_alive && all_tanks[i].crew_health() && all_tanks[i].hull_durability())	//если угол меньший, и он живой
			min_angle_alive = angle;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////FIRING/////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	Tank *enemy = NULL;
	double measure_enemy = 0;
	for (size_t i = 0; i < EnemiesToAttack.size(); ++i)
	{
		double measure = MeasureEnemy_ToAtack (&self, &world, &(all_tanks[EnemiesToAttack[i]]));
		{
			if (measure > measure_enemy)
			{
				enemy = &(all_tanks[EnemiesToAttack[i]]);
				measure_enemy = measure;
			}
		}
	}
	if (enemy)
	{		//определил врага
		if (self.remaining_reloading_time () / self.reloading_time() <= 0.7)
		{
			double angle = self.GetTurretAngleTo (*enemy);
			double dist = self.GetDistanceTo (*enemy);
			if (fabs (angle) < MIN_FIRING_ANGLE)
			{
				if (self.remaining_reloading_time() == 0)
				{
					if (dist * fabs (sin (angle)) <= enemy->height()/2)
						move.set_fire_type (PREMIUM_PREFERRED);
					else
						move.set_fire_type (REGULAR_FIRE);
				}
			}
			else if (angle > 0)
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

	if (world.tick() < self.reloading_time() / 2)
	{
		double dist_tl = self.GetDistanceTo (0,					0);
		double dist_tr = self.GetDistanceTo (world.width(),		0);
		double dist_br = self.GetDistanceTo (world.width(),		world.height());
		double dist_bl = self.GetDistanceTo (0,					world.height());
		if (min (dist_tl, dist_tr) < min (dist_bl, dist_br))
		{
			if (min (dist_tl, dist_bl) < min (dist_tr, dist_br))
				GoTo (move, self.GetAngleTo (0, 0));
			else
				GoTo (move, self.GetAngleTo (0, world.height()));
		}
		else
		{
			if (min (dist_tl, dist_bl) < min (dist_tr, dist_br))
				GoTo (move, self.GetAngleTo (world.width(), 0));
			else
				GoTo (move, self.GetAngleTo (world.width(), world.height()));
		}
		return;
	}

	Bonus *goal = NULL;
	double measure_goal = 0;
	double measure_health = MeasureSelf_Health(&self, &world);
	double measure_shield = MeasureSelf_Shield(&self, &world);
	double measure_ammo = MeasureSelf_Ammo(&self, &world);
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
				measure = measure_health * pow (max_bonus_dist_health / bonuses[i].GetDistanceTo (self), 1.7);
				break;
			}
		case REPAIR_KIT:
			{
				measure = measure_shield * pow (max_bonus_dist_shield / bonuses[i].GetDistanceTo (self), 1.5);
				break;
			}
		case AMMO_CRATE:
			{
				measure = measure_ammo * pow (max_bonus_dist_ammo / bonuses[i].GetDistanceTo (self), 1.1);
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
	bool move_on = false;
	if (goal)
	{
		Shell *danger = NULL;
		double danger_measure = 0;
		for (size_t i = 0; i <shells.size(); ++i)
		{
			double measure = MeasureShell_Defend (&self, &world, &shells[i]);
			if (measure > danger_measure)
			{
				danger = &shells[i];
				danger_measure = measure;
			}
		}
		if (danger && danger_measure > MIN_DANGER_LEVEL)
			GoFrom (move, self.GetAngleTo (*danger), self.GetAngleTo (*goal));
		else
			GoTo (move, self.GetAngleTo (*goal));
	}
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return MEDIUM;
}