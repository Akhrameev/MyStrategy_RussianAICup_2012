#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

using namespace model;
using namespace std;

const double MIN_ANGLE = M_PI / 6.0; // ���� � 30 ��������
const double MAX_ANGLE = M_PI /*/ 6.0*/; // ���� � 180 ��������

void MyStrategy::Move(Tank self, World world, model::Move& move) {
	move.set_fire_type(PREMIUM_PREFERRED);
	vector<Tank> all_tanks = world.tanks();             // ������� ������ ���� �������
    double min_dist_to_bonus = 1E20;
    size_t selected_bonus = all_tanks.size();
    for(size_t i = 0; i < all_tanks.size(); ++i) {         // ���������� ����� �� ������
       Tank bonus = all_tanks[i];
       double dist_to_bonus = self.GetDistanceTo(bonus);    // ������ ���������� �� ������
	   if ((dist_to_bonus > 0 ) /*&& (dist_to_bonus < min_dist_to_bonus)*/ && (bonus.crew_health() > 0) && (bonus.speed_x() || bonus.speed_y() )) {             // ������ ���������
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
       double angle_to_bonus = self.GetAngleTo(all_tanks[selected_bonus]); // ������ ���� �� ������

       if (angle_to_bonus < MAX_ANGLE) {         // ���� ���� ������ �������������,
		   move.set_turret_turn (-angle_to_bonus);
	   }
	   else
	   {
		   move.set_turret_turn (angle_to_bonus);
	   }
    }
}

TankType MyStrategy::SelectTank(int tank_index, int team_size) {
    return HEAVY;
}