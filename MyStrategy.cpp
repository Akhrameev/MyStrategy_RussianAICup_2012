#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>

using namespace model;
using namespace std;

int my_tank_index = 0;
int CurrentEnemy = 0;
std::string enemy_name = "";
long enemy_id = -1;

const double MIN_ANGLE = M_PI / 6.0; 
const double MAX_ANGLE = M_PI; 
const double MIN_FIRING_ANGLE = M_PI / 36.0; 

void MyStrategy::Move(Tank self, World world, model::Move& move) 
{
	move.set_fire_type(NONE);
	vector <Tank> all_tanks = world.tanks();
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
		if ((i != my_tank_index) && (all_tanks[i].angular_speed() || all_tanks[i].speed_x() || all_tanks[i].speed_y()) && (all_tanks[i].crew_health()))
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
		double turret_angle_to = self.GetTurretAngleTo (aim);
		if (abs (turret_angle_to) >= MIN_FIRING_ANGLE)
		{	//не буду шмалять, буду крутить пушку
			if (self.turret_max_relative_angle())
			{	//если могу крутить пушку
				if (turret_angle_to > 0)
					move.set_turret_turn (self.turret_turn_speed());
				else
					move.set_turret_turn (-self.turret_turn_speed());
			}
			else
			{	//иначе буду крутить танк
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
			}
		}
		else
		{	//постараюсь шмалять
			if (!self.remaining_reloading_time())
			{
				move.set_fire_type(PREMIUM_PREFERRED);
			}
		}
	}
	vector <Shell> shells = world.shells();
	vector <int> shells_to_me;
	double min_dist_to_shell = 1E20;
	int closest_shell = -1;
	for (size_t i = 0; i < shells.size(); ++i)
	{
		std::string my_name = self.player_name();
		if (shells[i].player_name() != my_name)
		{
			double dist = self.GetDistanceTo (shells[i]);
			if (dist < min_dist_to_shell)
			{
				closest_shell = i;
				min_dist_to_shell = dist;
			}
		}
	}
	if (closest_shell >= 0)
	{
		if (min_dist_to_shell < self.width() + self.height())
		{	//попробую уехать так, чтобы в меня не попал заряд
			double my_tank_angle_to = self.GetAngleTo (shells[closest_shell]);
			if (my_tank_angle_to > 0)
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
	my_tank_index = tank_index;
    return HEAVY;
}