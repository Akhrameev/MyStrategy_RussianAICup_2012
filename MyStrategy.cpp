#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>

using namespace model;
using namespace std;

int CurrentEnemy = 0;
std::string enemy_name = "";
long long enemy_id = -1;

const double MIN_ANGLE = M_PI / 6.0; 
const double MAX_ANGLE = M_PI; 
const double MIN_FIRING_ANGLE = M_PI / 72.0; 
const double MIN_FLEE_ANGLE = M_PI / 6.0;

enum OBJECT {TANK, SHELL, BONUS};

class PrevData
{
public:
	OBJECT obj;
	vector <Unit> units;
};

bool is_aim (Unit dot1, Unit dot2, Unit aim)
{
	double x1 = dot1.x(), x2 = dot2.x(), y1 = dot2.y(), y2 = dot2.y();
	bool center			= ((aim.y() - y1) / (y2 - y1) >= (aim.x() - x1) / (x2 - x1));
	bool right_up		= ((aim.y() - aim.height() - y1) / (y2 - y1) >= (aim.x() + aim.height() - x1) / (x2 - x1));
	bool right_down		= ((aim.y() + aim.height() - y1) / (y2 - y1) >= (aim.x() + aim.height() - x1) / (x2 - x1));
	bool left_up		= ((aim.y() - aim.height() - y1) / (y2 - y1) >= (aim.x() - aim.height() - x1) / (x2 - x1));
	bool left_down		= ((aim.y() + aim.height() - y1) / (y2 - y1) >= (aim.x() - aim.height() - x1) / (x2 - x1));
	return (center == right_up == right_down == left_up == left_down);
}

double line_tan (Unit dot1, Unit dot2)
{
	return ((double (dot2.x()) - dot1.x()) / (dot2.y() - double (dot1.y())));
}

vector <PrevData> PreviousData_Tanks;
vector <PrevData> PreviousData_Shells;

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	vector <Tank> all_tanks = world.tanks();
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		bool unit_placed = false;
		for (size_t j = 0; j < PreviousData_Tanks.size(); ++j)
		{
			if ((PreviousData_Tanks [j].obj == TANK) && (PreviousData_Tanks[j].units[0].id() == all_tanks[i].id()))
			{
				PreviousData_Tanks[j].units.push_back (all_tanks[i]);
				unit_placed = true;
				break;
			}
		}
		if (!unit_placed)
		{
			PrevData prev;
			prev.obj = TANK;
			prev.units.push_back (all_tanks[i]);
			PreviousData_Tanks.push_back (prev);
		}
	}
	vector <Shell> shells = world.shells();
	for (size_t i = 0; i < shells.size(); ++i)
	{
		if (shells[i].id() == self.id())
			continue;
		bool unit_placed = false;
		for (size_t j = 0; j < PreviousData_Shells.size(); ++j)
		{
			if ((PreviousData_Shells [j].obj == SHELL) && (PreviousData_Shells[j].units[0].id() == shells[i].id()))
			{
				PreviousData_Shells[j].units.push_back (shells[i]);
				unit_placed = true;
				break;
			}
		}
		if (!unit_placed)
		{
			PrevData prev;
			prev.obj = SHELL;
			prev.units.push_back (shells[i]);
			PreviousData_Shells.push_back (prev);
		}
	}
	move.set_fire_type(NONE);
	vector <int> EnemiesToAttack;
	CurrentEnemy = -1;
	for (size_t i = 0; i < all_tanks.size(); ++i)
		if (all_tanks[i].player_name() == enemy_name)
		{
			if ((CurrentEnemy < 0) && (all_tanks[i].id() == enemy_id))
				CurrentEnemy = i;
			if (CurrentEnemy < 0)
				CurrentEnemy = i;
		}
	if (CurrentEnemy >= 0 && !all_tanks[CurrentEnemy].crew_health())
		CurrentEnemy = -1;
	for (size_t i = 0; i < all_tanks.size(); ++i)
	{
		if ((all_tanks[i].id() != self.id()) && (all_tanks[i].angular_speed() || all_tanks[i].speed_x() || all_tanks[i].speed_y()) && (all_tanks[i].crew_health()))
			EnemiesToAttack.push_back (i);
	}
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
				move.set_right_track_power(-0.5);
			}
			else
			{
				move.set_left_track_power (-0.5);
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
	}
	/*
	if (closest_shell >= 0)	//попытки уворавиваться от снарядов
	{
		Shell current = shells[closest_shell];
		if (abs (atan (self.GetAngleTo (current)) - atan(-tan_closest)) < MIN_FLEE_ANGLE)
		{	//пора давать дёру
			if (self.GetAngleTo (current) > 0)
			{
				move.set_left_track_power (-1.0);
				move.set_right_track_power(-1.0);
			}
			else
			{
				move.set_left_track_power (1.0);
				move.set_right_track_power(1.0);
			}
		}
		else
		{	//необходимо повернуться (если до следующего моего выстрела меньше 5% времени)
			if (100 * self.remaining_reloading_time() / self.reloading_time() > 5)
				{
				double my_tank_angle_to = self.GetAngleTo (current);
				if (my_tank_angle_to > 0)
				{
					move.set_left_track_power (1.0);
					move.set_right_track_power(-0.5);
				}
				else
				{
					move.set_left_track_power (-0.5);
					move.set_right_track_power(1.0);
				}
			}
		}
	}*/

	Unit *closest_heal = NULL;
	Unit *closest_armor = NULL;;
	Unit *closest_turtoise = NULL;
	vector <Bonus> bonuses = world.bonuses();
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
				else
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
				else
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
				else
					closest_turtoise = &bonuses[i];
				break;
			}
		default:
			break;
		}
	}

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