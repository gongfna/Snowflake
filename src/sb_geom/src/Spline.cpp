/*
 * Created By: Gareth Ellis
 * Created On: January 27, 2018
 * Description: TODO
 */

#include "sb_geom/Spline.h"

using namespace sb_geom;

// TODO: Probably don't want to be passing points in by reference
Spline::Spline(std::vector<Point2D> &points):
    interpolation_points(points)
{
    interpolate();
}

double Spline::approxLength(int num_sample_points) {
    double length = 0;

    Point2D prev_point(this->(0));
    Point2D curr_point;
    double increment = 1/num_sample_points;
    for (int i = 0; i < 1; i++){
        curr_point = this->(i * increment);
        // Calculate distance between current and previous point
        double dx = curr_point.x() - prev_point.x();
        double dy = curr_point.y() - prev_point.y();
        length += std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));

        // Update the prev point
        prev_point = curr_point;
    }

    return length;
}
Point2D Spline::operator()(double u){
    if (u < 0 || u > 1){
        // Throw an exception if u is outside of [0,1]
        std::string err_msg = "u must be between 0 and 1, given: " + std::to_string(u);
        throw std::out_of_range(err_msg);
    }
    // Scale the given u value from [0,1] -> [0,n] where n is the number of points
    // the spline interpolates through
    u = u * (interpolation_points.size()-1);
    return Point2D(alglib::spline1dcalc(x_interpolant, u), alglib::spline1dcalc(y_interpolant, u));
}

void Spline::interpolate() {
    // TODO: Laymans comment here
    // Parametrize the points in terms of u in [0,n] where n=points.size()
    alglib::real_1d_array x;
    alglib::real_1d_array y;
    alglib::real_1d_array u;
    x.setlength(interpolation_points.size());
    y.setlength(interpolation_points.size());
    u.setlength(interpolation_points.size());
    for (int i = 0; i < interpolation_points.size(); i++){
        Point2D& point = interpolation_points[i];
        x[i] = point.x();
        y[i] = point.y();
        u[i] = i;
    }
    for (int i = 0; i < interpolation_points.size(); i++){
        std::cout << x[i] << " " << y[i] << " " << u[i] << std::endl;
    }

    alglib::spline1dbuildakima(u, x, x_interpolant);
    alglib::spline1dbuildakima(u, y, y_interpolant);
}

