/*
 * Created By: Robyn Castro
 * Created On: September 22, 2016
 * Description: The vision decision node, takes in an image from the robot's
 *              camera and produces a recommended twist message
 */
#include <VisionDecision.h>

// The constructor
VisionDecision::VisionDecision(int argc, char** argv, std::string node_name) {
    ros::init(argc, argv, node_name);

    // Setup NodeHandles
    ros::NodeHandle nh;
    ros::NodeHandle private_nh("~");

    // Setup Subscriber(s)
    std::string camera_image_topic_name = "/vision/filtered_image";
    uint32_t queue_size                 = 1;
    image_subscriber                    = private_nh.subscribe(
    camera_image_topic_name, queue_size, &VisionDecision::imageCallBack, this);

    // Setup Publisher(s)
    std::string twist_topic = private_nh.resolveName("twist");
    twist_publisher =
    nh.advertise<geometry_msgs::Twist>(twist_topic, queue_size);

    // Get Param(s)
    SB_getParam(
    private_nh, "angular_vel_multiplier", angular_velocity_multiplier, 1.0);
    SB_getParam(private_nh, "angular_vel_cap", angular_velocity_cap, 1.0);
    SB_getParam(
    private_nh, "rolling_average_constant", rolling_average_constant, 0.25);
    SB_getParam(
    private_nh, "percent_of_samples_needed", percent_of_samples_needed, 0.125);
    SB_getParam(
    private_nh, "percent_of_image_sampled", percent_of_image_sampled, 0.25);
    SB_getParam(private_nh, "move_away_threshold", move_away_threshold, 25.0);
    SB_getParam(private_nh, "confidence_threshold", confidence_threshold, 60.0);
    SB_getParam(
    private_nh, "percent_of_white_needed", percent_of_white_needed, 0.05);
}

// This is called whenever a new message is received
void VisionDecision::imageCallBack(
const sensor_msgs::Image::ConstPtr& image_scan) {
    // Deal with new messages here
    geometry_msgs::Twist twistMsg;

    double confidence_value = 0;
    // Decide how much to turn
    int relativeAngle =
    getDesiredAngle(image_scan->height * percent_of_image_sampled,
                    image_scan,
                    rolling_average_constant,
                    percent_of_image_sampled,
                    percent_of_samples_needed,
                    confidence_value,
                    move_away_threshold,
                    percent_of_white_needed);

    // Initialize linear velocities to 0
    twistMsg.linear.y = 0;
    twistMsg.linear.z = 0;

    // Initialize x and y angular velocities to 0
    twistMsg.angular.x = 0;
    twistMsg.angular.y = 0;

    // Decide how fast to move
    twistMsg.linear.x = getDesiredLinearSpeed(relativeAngle);

    // Decide how fast to turn
    twistMsg.angular.z =
    -angular_velocity_multiplier * getDesiredAngularSpeed(relativeAngle);

    // Cap the absolute value of the turning velocity
    if (fabs(twistMsg.angular.z) > angular_velocity_cap)
        twistMsg.angular.z =
        angular_velocity_cap * twistMsg.angular.z / fabs(twistMsg.angular.z);

    // Publish the twist message
    publishTwist(twistMsg);
}

void VisionDecision::publishTwist(geometry_msgs::Twist twist) {
    twist_publisher.publish(twist);
}

/* Functions to determine robot movement */

int VisionDecision::getDesiredAngle(
double numSamples,
const sensor_msgs::Image::ConstPtr& image_scan,
double rolling_average_constant,
double percent_of_image_sampled,
double percent_of_samples_needed,
double& confidence_value,
double move_away_threshold,
double percent_of_white_needed) {
    int row, col;
    int whiteCount   = 0;
    confidence_value = 0;

    // Check if there is a white line in the way of the robot
    for (row = 0; row < image_scan->height; row++) {
        for (col = 0; col < image_scan->width; col++)
            if (image_scan->data[row * image_scan->width + col] != 0)
                whiteCount++;
    }

    double valid_samples;

    int leftToRightAngle = getAngleOfLine(false,
                                          numSamples,
                                          image_scan,
                                          rolling_average_constant,
                                          percent_of_samples_needed,
                                          valid_samples);

    double leftConfidence = getConfidence(image_scan,
                                          percent_of_image_sampled,
                                          percent_of_samples_needed,
                                          valid_samples);

    double rightToLeftAngle = getAngleOfLine(true,
                                             numSamples,
                                             image_scan,
                                             rolling_average_constant,
                                             percent_of_samples_needed,
                                             valid_samples);

    double rightConfidence = getConfidence(image_scan,
                                           percent_of_image_sampled,
                                           percent_of_samples_needed,
                                           valid_samples);

    int desiredAngle;

    if (rightConfidence > leftConfidence) {
        desiredAngle     = rightToLeftAngle;
        confidence_value = rightConfidence;
    } else {
        desiredAngle     = leftToRightAngle;
        confidence_value = leftConfidence;
    }

    if (fabs(desiredAngle) <= move_away_threshold)
        desiredAngle = moveAwayFromLine(image_scan);

    // If there is a perpendicular line in front of the robot, stop.
    if (isPerpendicular(image_scan)) confidence_value = 0;

    double num_of_white_needed =
    image_scan->height * image_scan->width * percent_of_white_needed;
    if (whiteCount < num_of_white_needed)
        desiredAngle = moveAwayFromLine(image_scan);

    return desiredAngle;
}

int VisionDecision::getAngleOfLine(
bool rightSide,
double numSamples,
const sensor_msgs::Image::ConstPtr& image_scan,
double rolling_average_constant,
double percent_of_samples_needed,
double& validSamples) {
    // initialization of local variables.
    double imageHeight = image_scan->height;
    int incrementer;
    int startingPos = 0;
    validSamples    = 0;

    // Assign garbage values
    int bottomRow = -1;
    double x1     = -1;

    // Initialize how and where to parse.
    incrementer =
    initializeIncrementerPosition(rightSide, image_scan, &startingPos);

    // starts parsing from the right and finds where the lowest white line
    // begins.
    for (int i = imageHeight - 1; i > 0; i--) {
        int startPixel =
        getEdgePixel(startingPos, incrementer, i, image_scan, true);
        if (startPixel != -1 && bottomRow == -1) {
            // Each slope will be compared to the bottom point of the lowest
            // white line
            bottomRow = i;
            x1 = getMiddle(startingPos, bottomRow, rightSide, image_scan);
            break;
        }
    }

    double currentAngle = 0;

    // Finds slopes (corresponding to number of samples given) then returns the
    // sum of all slopes
    // also counts how many of them are valid.
    for (double division = 1;
         (division < numSamples) && (bottomRow - division > 0);
         division++) {
        double yCompared = bottomRow - division;
        double xCompared =
        getMiddle(startingPos, (int) yCompared, rightSide, image_scan);

        double foundAngle;
        double foundSlope;

        if (xCompared != -1 && x1 != -1) {
            // slope is valid, find the angle compared the the positive y-axis.
            foundSlope = -(xCompared - x1) / (yCompared - bottomRow);
            foundAngle = atan(foundSlope);
            // increment amount of valid samples
            validSamples++;

            if (DEBUG)
                printf(
                "curAngle: %f, x1: %f, bottomRow: %d, xCompared: %f, "
                "yCompared: %f, "
                "Found Angle: %f, Valid: %f \n",
                currentAngle * 180 / M_PI,
                x1,
                bottomRow,
                xCompared,
                yCompared,
                foundAngle * 180 / M_PI,
                validSamples);

            // Update the current angle if the change is not too sudden.
            // (Found angle does not exceed a right angle turn of the current
            // angle)
            if (fabs(currentAngle - foundAngle) * 180 / M_PI < 90)
                currentAngle = rolling_average_constant * foundAngle +
                               (1 - rolling_average_constant) * currentAngle;
            else
                validSamples--;
        }
    }

    if (x1 == -1) validSamples = 0;

    return (int) (currentAngle * 180.0 / M_PI); // returns the angle in degrees
}

double VisionDecision::getDesiredAngularSpeed(double desiredAngle) {
    // the higher the desired angle, the higher the angular speed
    if (desiredAngle == STOP_SIGNAL_ANGLE) return 0;

    double mappedSpeed = pow(desiredAngle, 2.0);

    mappedSpeed = mappedSpeed / 10000.0;

    if (desiredAngle < 0) mappedSpeed = mappedSpeed * (-1.0);

    return mappedSpeed;
}

double VisionDecision::getDesiredLinearSpeed(double desiredAngle) {
    double speedToMap = abs((int) desiredAngle);

    if (desiredAngle == STOP_SIGNAL_ANGLE) return 0;

    // the higher the desired angle the lower the linear speed.
    return 1 - mapRange(speedToMap, 0, 90, 0, 1);
}

/* Helper functions for functions that determine robot movement. */

int VisionDecision::getMiddle(int startingPos,
                              int row,
                              bool rightSide,
                              const sensor_msgs::Image::ConstPtr& image_scan) {
    int incrementer;
    int startPixel, endPixel;

    // Depending on chosen side, determine where to start
    // and how to iterate.
    if (rightSide)
        incrementer = -1;
    else
        incrementer = 1;

    // Find first pixel of the white line in a certain row.
    startPixel =
    VisionDecision::getEdgePixel(startingPos, incrementer, row, image_scan, 1);

    // Find last pixel of the white line in a certain row.
    endPixel =
    VisionDecision::getEdgePixel(startPixel, incrementer, row, image_scan, 0);

    // Return average of the two pixels.
    return (startPixel + endPixel) / 2;
}

int VisionDecision::getEdgePixel(int startingPos,
                                 int incrementer,
                                 int row,
                                 const sensor_msgs::Image::ConstPtr& image_scan,
                                 bool isStartPixel) {
    // Initialization of local variables
    int column                 = startingPos;
    int whiteVerificationCount = 0;
    int blackVerificationCount = 0;

    // Initialize these to be garbage values
    int toBeChecked = -1;

    // Parse through image horizontally to find a valid pixel
    while (column < image_scan->width && column >= 0) {
        // If white pixel found start verifying if proper start.
        if ((image_scan->data[row * image_scan->width + column] != 0 &&
             isStartPixel) ||
            (image_scan->data[row * image_scan->width + column] == 0 &&
             !isStartPixel)) {
            blackVerificationCount = 0;
            // This pixel is what we are checking
            if (toBeChecked == -1) { toBeChecked = column; }

            // Determine whether toBeChecked is noise
            whiteVerificationCount++;
            if (whiteVerificationCount == NOISE_MAX) return toBeChecked;
        } else {
            blackVerificationCount++;
            if (blackVerificationCount >= NOISE_MAX) {
                whiteVerificationCount =
                0; // Reset verification if black pixel.
                toBeChecked = -1;
            }
        }
        // Go to next element
        column += incrementer;
    }

    // No value found return an error.
    return -1;
}

int VisionDecision::initializeIncrementerPosition(
bool rightSide,
const sensor_msgs::Image::ConstPtr& image_scan,
int* startingPos) {
    // Decides how to parse depending on if rightSide is true or false.
    if (rightSide) {
        // starts at right side then increments to the left
        *startingPos = image_scan->width - 1;
        return -1;
    } else {
        // starts at left side then increments to th right
        *startingPos = 0;
        return 1;
    }
}

bool VisionDecision::isPerpendicular(
const sensor_msgs::Image::ConstPtr& image_scan) {
    int leftSidePixel, rightSidePixel = -1;
    int i, j;

    for (i = 0; i < image_scan->width; i++) {
        leftSidePixel = getVerticalEdgePixel(image_scan, i);
        if (leftSidePixel != -1) break;
    }

    for (j = image_scan->width - 1; j > 0; j--) {
        rightSidePixel = getVerticalEdgePixel(image_scan, j);
        if (rightSidePixel != -1) break;
    }

    if (rightSidePixel == 0 && leftSidePixel == 0) return 0;

    return (fabs(rightSidePixel - leftSidePixel) < image_scan->height / 10 &&
            fabs(j - i) > image_scan->width / 10);
}

int VisionDecision::getVerticalEdgePixel(
const sensor_msgs::Image::ConstPtr& image_scan, int column) {
    int row;
    int whiteVerificationCount = 0;
    int blackVerificationCount = 0;

    // Initialize these to be garbage values
    int toBeChecked = -1;

    // Parse vertically to find a valid starting white pixel.
    for (row = image_scan->height - 1; row >= 0; row--) {
        // If white pixel found start verifying if proper start.
        if ((image_scan->data[row * image_scan->width + column] != 0)) {
            blackVerificationCount = 0;
            // This pixel is what we are checking
            if (toBeChecked == -1) toBeChecked = row;

            // Determine whether toBeChecked is noise
            whiteVerificationCount++;
            if (whiteVerificationCount == NOISE_MAX) return toBeChecked;
        } else {
            blackVerificationCount++;
            if (blackVerificationCount == NOISE_MAX) {
                whiteVerificationCount =
                0; // Reset verification if black pixel.
                toBeChecked = -1;
            }
        }
    }
}

int VisionDecision::getLeftToRightPixelRatio(
const sensor_msgs::Image::ConstPtr& image_scan) {
    int leftCount  = 0;
    int rightCount = 0;

    for (int row = 0; row < image_scan->height; row++) {
        for (int column = 0; column < image_scan->width; column++) {
            if (image_scan->data[row * image_scan->width + column] != 0 &&
                column <= image_scan->width / 2) {
                leftCount++;
            }
            if (image_scan->data[row * image_scan->width + column] != 0 &&
                column > image_scan->width / 2)
                rightCount++;
        }
    }

    return rightCount - leftCount;
}

int VisionDecision::moveAwayFromLine(
const sensor_msgs::Image::ConstPtr& image_scan) {
    if (getLeftToRightPixelRatio(image_scan) < 0)
        return 45;
    else
        return -45;
}

double VisionDecision::mapRange(
double x, double inMin, double inMax, double outMin, double outMax) {
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

double
VisionDecision::getConfidence(const sensor_msgs::Image::ConstPtr& image_scan,
                              double percent_of_image_sampled,
                              double percent_of_samples_needed,
                              double valid_samples) {
    double samples_needed = image_scan->height * percent_of_samples_needed;

    if (valid_samples > samples_needed) valid_samples = samples_needed;
    return mapRange(valid_samples, 0, samples_needed, 0, 100);
}