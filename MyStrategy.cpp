#include "MyStrategy.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

using namespace model;
using namespace std;

const double MIN_ANGLE = M_PI / 6.0; // угол в 30 градусов
const double MAX_ANGLE = M_PI /*/ 6.0*/; // угол в 180 градусов

void MyStrategy::Move(Tank self, World world, model::Move& move) {
	move.set_fire_type(PREMIUM_PREFERRED);
	vector<Tank> all_tanks = world.tanks();             // получим список всех бонусов
    double min_dist_to_bonus = 1E20;
    size_t selected_bonus = all_tanks.size();
    for(size_t i = 0; i < all_tanks.size(); ++i) {         // перебираем бонус из списка
       Tank bonus = all_tanks[i];
       double dist_to_bonus = self.GetDistanceTo(bonus);    // найдем расстояние до бонуса
	   if ((dist_to_bonus > 0 ) /*&& (dist_to_bonus < min_dist_to_bonus)*/ && (bonus.crew_health() > 0) && (bonus.speed_x() || bonus.speed_y() )) {             // найдем ближайший
         min_dist_to_bonus = dist_to_bonus;
         selected_bonus = i;
       }
    }

    if (selected_bonus != all_tanks.size()) {
       double angle_to_bonus = self.GetAngleTo(all_tanks[selected_bonus]); // найдем угол до бонуса

       if (angle_to_bonus > MIN_ANGLE) {         // если угол сильно положительный,
         move.set_left_track_power(0.75);      // то будем разворачиваться,
         move.set_right_track_power(-1.0);        // поставив противоположные силы гусеницам.
       } else if (angle_to_bonus < -MIN_ANGLE) {  // если угол сильно отрицательный,
         move.set_left_track_power(-1.0);         // будем разворачиваться
         move.set_right_track_power(0.75);     // в противоположную сторону.
       } else {
         move.set_left_track_power(1.0);         // если угол не больше 30 градусов
         move.set_right_track_power(1.0);        // поедем максимально быстро вперед 
       }
    }
	    if (selected_bonus != all_tanks.size()) {
       double angle_to_bonus = self.GetAngleTo(all_tanks[selected_bonus]); // найдем угол до бонуса

       if (angle_to_bonus < MAX_ANGLE) {         // если угол сильно положительный,
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