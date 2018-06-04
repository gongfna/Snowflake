/*
 * Created By: Min Gyo Kim
 * Created On: May 23rd 2018
 * Description: Implementation of Path Finder Node
 *              Once the node has received both occupancy grid and the goal
 * point,
 *              it retrieves the position of the robot through the tf tree, and
 *              calls PathFinder's static methods to calculate the path.
 *              Then it publishes the path.
 */

#include <PathFinderNode.h>
#include "PathFinderUtils.h"

PathFinderNode::PathFinderNode(int argc, char** argv, std::string node_name) {
    ros::init(argc, argv, node_name);
    ros::NodeHandle nh;
    ros::NodeHandle private_nh("~");

    // TODO: we should have a "Path" frame, not a "map" frame
    // TODO: Global frame should really just be the frame of the occ grid
    SB_getParam(private_nh,
                std::string("global_frame_name"),
                this->_global_frame_name,
                std::string("/map"));
    SB_getParam(private_nh,
                std::string("base_frame_name"),
                this->_base_frame_name,
                std::string("/base_link"));
    SB_getParam(private_nh,
                std::string("use_dijkstra"),
                this->_use_dijkstra,
                true);
    SB_getParam(private_nh,
                std::string("blocked_cell_threshold"),
                this->_blocked_cell_threshold,
                50);

    std::cout << "**************************************" << std::endl;
    std::cout << "use_dijkstra is " << this->_use_dijkstra << std::endl;
    std::cout << "blocked_cell_threshold is " << this->_blocked_cell_threshold << std::endl;
    std::cout << "**************************************" << std::endl;

    SB_getParam(private_nh,
                std::string("path_update_rate"),
                this->_path_update_rate,
                (float) 0.1);
     std::string grid_subscriber_topic = "/occupancy_grid";

    uint32_t queue_size                    = 1;
    this->grid_subscriber             = nh.subscribe(grid_subscriber_topic,
                                         queue_size,
                                         &PathFinderNode::occupancyGridCallback,
                                         this);

    std::string goal_subscriber_topic = "/goal";
    this->goal_subscriber             = nh.subscribe(
    goal_subscriber_topic, queue_size, &PathFinderNode::goalCallback, this);

    std::string topic_to_publish_to = "path";
    queue_size             = 1;
    this->publisher =
    private_nh.advertise<nav_msgs::Path>(topic_to_publish_to, queue_size);

    this->_listener = new tf::TransformListener();

    // Setup timer callback for updating the path
    timer = nh.createTimer(ros::Duration(this->_path_update_rate),
                           &PathFinderNode::updatePathCallback,
                           this);

}

void PathFinderNode::occupancyGridCallback(const nav_msgs::OccupancyGrid grid) {
    this->_grid           = grid;
    this->_receivied_grid = true;
}

void PathFinderNode::goalCallback(const geometry_msgs::PointStamped goal) {
    this->_goal          = goal.point;
    this->_received_goal = true;
}

void PathFinderNode::updatePathCallback(const ros::TimerEvent& event)
{
    // Update the path
    if (this->_receivied_grid && this->_received_goal) { publishPath(); }
}
void PathFinderNode::publishPath() {
    tf::StampedTransform transform;

    try {
        this->_listener->lookupTransform(this->_global_frame_name,
                                         this->_base_frame_name,
                                         ros::Time(0),
                                         transform);
    } catch (tf::TransformException ex) {
        // If we can't lookup the tf, then don't publish path
        ROS_WARN_STREAM(
        "Could not lookup tf between " << this->_global_frame_name << " and "
                                       << this->_base_frame_name);
        ROS_WARN_STREAM(ex.what());
        return;
    }

    geometry_msgs::Point start;
    start.x = transform.getOrigin().x();
    start.y = transform.getOrigin().y();

    // If we have an empty map, we can just go straight towards the goal
    if (this->_grid.info.height == 0 || this->_grid.info.width == 0) {
        ROS_INFO("No map, going right to goal");

        // Transform point to robot frame
//        geometry_msgs::PointStamped stamped_point;
//        stamped_point.point = this->_goal;
//        stamped_point.header.frame_id = this->_global_frame_name;
//        stamped_point.header.stamp = ros::Time(0);
//        geometry_msgs::PointStamped transformed_point;
//        _listener->transformPoint(this->_base_frame_name, stamped_point, transformed_point);

        tf::Vector3 goal_map_vector = PathFinderUtils::pointToVector(this->_goal);
        tf::Vector3 goal_local_vector = transform * goal_map_vector;

        geometry_msgs::Point goal_point = PathFinderUtils::vectorToPoint(goal_local_vector);

        geometry_msgs::PoseStamped goal_pose = PathFinderUtils::constructPoseStamped(
                goal_point, 0
        );
        nav_msgs::Path path_short;
        path_short.poses.push_back(goal_pose);

        this->publisher.publish(path_short);

//        ROS_INFO_STREAM("IN: " << stamped_point);
//        ROS_INFO_STREAM("OUT: " << transformed_point);

        return;
    }

    nav_msgs::Path path =
    PathFinder::calculatePath(start, this->_goal, this->_grid, this->_blocked_cell_threshold, this->_use_dijkstra);

    // TODO: We should probaly be publisshing this in either the map frame or it's own frame
    path.header.frame_id = this->_global_frame_name;
    this->publisher.publish(path);

}
