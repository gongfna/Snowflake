/*
 * Created By: Gareth Ellis
 * Created On: January 27, 2018
 * Description: TODO
 */

#ifndef SB_GEOM_POLYNOMIALSEGMENT_H
#define SB_GEOM_POLYNOMIALSEGMENT_H

#include "sb_geom_msgs/Polynomial.h"
#include "sb_geom/Polynomial.h"

namespace sb_geom {

    // A segment of a polnomial line
    class PolynomialSegment : public Polynomial {
    public:
        // TODO: COmment?
        PolynomialSegment() :
                _x_min(0),
                _x_max(0)
        {};

        // TODO: Test
        /**
         * Construct a Polynomial Line Segment
         * @param coefficients the coefficients of the polynomial line
         * @param x_min the min x value of the segment
         * @param x_max the max x value of the segment
         */
        PolynomialSegment(std::vector<double> &coefficients, double x_min, double x_max) :
                Polynomial(coefficients),
                _x_min(x_min),
                _x_max(x_max) {};

        // TODO: Test
        /**
         * Get the min x value of this line segment
         * @return the min x value of this line segment
         */
        double &x_min() { return _x_min; }

        // TODO: Test
        /**
         * Get the max x value of this line segment
         * @return the max x value of this line segment
         */
        double &x_max() { return _x_max; }

    private:
        // The min/max extents of the line segment
        double _x_min, _x_max;
    };
}


#endif //SB_GEOM_POLYNOMIALSEGMENT_H
