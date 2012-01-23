/**
 * $Id: kin2pcl.cpp 134 2012-01-12 13:52:36Z spanel $
 *
 * Developed by dcgm-robotics@FIT group
 * Author: Rostislav Hulik (ihulik@fit.vutbr.cz)
 * Date: 11.01.2012 (version 1.0)
 *
 * License: BUT OPEN SOURCE LICENSE
 *
 * Description:
 * Module exports depth map images point cloud messages
 *
 */

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/CvBridge.h>

//PCL
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>

#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/features/normal_3d.h>
#include <pcl/surface/gp3.h>
#include <pcl/io/vtk_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl_ros/point_cloud.h>
//better opencv 2
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc_c.h>


#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <float.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>

#include "plane_det/filtering.h"
#include "plane_det/normals.h"


using namespace std;
using namespace cv;
using namespace pcl;
using namespace sensor_msgs;
using namespace message_filters;

sensor_msgs::PointCloud2 cloud_msg;
ros::Publisher pub;

void callback( const sensor_msgs::ImageConstPtr& dep, const CameraInfoConstPtr& cam_info)
{
	ros::Time begin = ros::Time::now();
	//  Debug info
	std::cerr << "Recieved frame..." << std::endl;
	std::cerr << "Cam info: fx:" << cam_info->K[0] << " fy:" << cam_info->K[4] << " cx:" << cam_info->K[2] <<" cy:" << cam_info->K[5] << std::endl;
	std::cerr << "Depth image h:" << dep->height << " w:" << dep->width << " e:" << dep->encoding << " " << dep->step << endl;

	//get image from message
	sensor_msgs::CvBridge bridge;
	cv::Mat depth = bridge.imgMsgToCv( dep );

	but_scenemodel::Normals normal(depth, cam_info);

	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);

	for (int i = 0; i < normal.m_points.rows; ++i)
	for (int j = 0; j < normal.m_points.cols; ++j)
	{
		Vec3f vector = normal.m_points.at<Vec3f>(i, j);

		//pcl::Vec
		cloud->push_back(pcl::PointXYZ(vector[0], vector[1], vector[2]));
	}

	pcl::VoxelGrid<pcl::PointXYZ> voxelgrid;
	voxelgrid.setInputCloud(cloud);
	voxelgrid.setLeafSize(0.05, 0.05, 0.05);
	voxelgrid.filter(*cloud);

	cloud->header.frame_id = "/openni_depth_frame";



	pub.publish(cloud);


	ros::Time end = ros::Time::now();
	std::cerr << "Computation time: " << (end-begin).nsec/1000000.0 << " ms." << std::endl;
	std::cerr << "=========================================================" << endl;
}

int main( int argc, char** argv )
{
	ros::init(argc, argv, "plane_detector");
	ros::NodeHandle n;

	// subscribe depth info
	message_filters::Subscriber<Image> depth_sub(n, "/cam3d/depth/image_raw", 1);
	message_filters::Subscriber<CameraInfo> info_sub_depth(n, "/cam3d/depth/camera_info", 1);

	pub = n.advertise<pcl::PointCloud<pcl::PointXYZ> > ("/point_cloud", 1);
	// sync images
	typedef sync_policies::ApproximateTime<Image, CameraInfo> MySyncPolicy;
	Synchronizer<MySyncPolicy> sync(MySyncPolicy(10), depth_sub, info_sub_depth);
	sync.registerCallback(boost::bind(&callback, _1, _2));



	std::cerr << "Kinect to point cloud converter initialized and listening..." << std::endl;
	ros::spin();

	return 1;
}
