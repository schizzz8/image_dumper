#include <iostream>
#include <ros/ros.h>
#include <sensor_msgs/CameraInfo.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include <lucrezio_simulation_environments/LogicalImage.h>

#include "tf/tf.h"
#include "tf/transform_listener.h"
#include "tf/transform_datatypes.h"

#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <lucrezio_semantic_perception/ImageBoundingBoxesArray.h>

bool keep_going=true;

class ImageDumper{
public:
  ImageDumper(ros::NodeHandle nh_):
    _nh(nh_),
    _logical_image_sub(_nh,"/gazebo/logical_camera_image",1),
    _depth_image_sub(_nh,"/camera/depth/image_raw",1),
    _rgb_image_sub(_nh,"/camera/rgb/image_raw", 1),
    _synchronizer(FilterSyncPolicy(10),_logical_image_sub,_depth_image_sub,_rgb_image_sub){

    _synchronizer.registerCallback(boost::bind(&ImageDumper::filterCallback, this, _1, _2, _3));

    ROS_INFO("Starting image dumper node!");
  }

  void filterCallback(const lucrezio_simulation_environments::LogicalImage::ConstPtr& logical_image_msg,
                      const sensor_msgs::Image::ConstPtr& depth_image_msg,
                      const sensor_msgs::Image::ConstPtr& rgb_image_msg){

    ROS_INFO("--------------------------");
    ROS_INFO("Executing filter callback!");
    ROS_INFO("--------------------------");
    std::cerr << std::endl;

    //Extract rgb and depth image from ROS messages
    cv_bridge::CvImageConstPtr rgb_cv_ptr,depth_cv_ptr;
    try{
      rgb_cv_ptr = cv_bridge::toCvShare(rgb_image_msg);
      depth_cv_ptr = cv_bridge::toCvShare(depth_image_msg);
    } catch (cv_bridge::Exception& e) {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    _rgb_image = rgb_cv_ptr->image.clone();
    int rgb_rows=_rgb_image.rows;
    int rgb_cols=_rgb_image.cols;
    std::string rgb_type=type2str(_rgb_image.type());
    ROS_INFO("Got %dx%d %s image",rgb_cols,rgb_rows,rgb_type.c_str());

    //saving rgb image
    cv::imwrite("rgb_image.png",_rgb_image);

    _depth_image = depth_cv_ptr->image.clone();
    int depth_rows=_depth_image.rows;
    int depth_cols=_depth_image.cols;
    std::string depth_type=type2str(_depth_image.type());
    ROS_INFO("Got %dx%d %s image",depth_cols,depth_rows,depth_type.c_str());

    //saving depth image
    cv::Mat temp;
    convert_32FC1_to_16UC1(temp,_depth_image);
    cv::imwrite("depth_image.pgm",temp);
    
    //stop the node
    keep_going=false;
  }

protected:
  ros::NodeHandle _nh;

  tf::TransformListener _listener;
  cv::Mat _rgb_image,_depth_image;
  Eigen::Isometry3f _depth_camera_transform,_inverse_depth_camera_transform;

  message_filters::Subscriber<lucrezio_simulation_environments::LogicalImage> _logical_image_sub;
  message_filters::Subscriber<sensor_msgs::Image> _depth_image_sub;
  message_filters::Subscriber<sensor_msgs::Image> _rgb_image_sub;
  typedef message_filters::sync_policies::ApproximateTime<lucrezio_simulation_environments::LogicalImage,
                                                          sensor_msgs::Image,
                                                          sensor_msgs::Image> FilterSyncPolicy;
  message_filters::Synchronizer<FilterSyncPolicy> _synchronizer;

private:
  Eigen::Isometry3f tfTransform2eigen(const tf::Transform& p){
    Eigen::Isometry3f iso;
    iso.translation().x()=p.getOrigin().x();
    iso.translation().y()=p.getOrigin().y();
    iso.translation().z()=p.getOrigin().z();
    Eigen::Quaternionf q;
    tf::Quaternion tq = p.getRotation();
    q.x()= tq.x();
    q.y()= tq.y();
    q.z()= tq.z();
    q.w()= tq.w();
    iso.linear()=q.toRotationMatrix();
    return iso;
  }

  std::string type2str(int type) {
    std::string r;
    uchar depth = type & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (type >> CV_CN_SHIFT);
    switch ( depth ) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
    }
    r += "C";
    r += (chans+'0');
    return r;
  }

  void convert_32FC1_to_16UC1(cv::Mat& dest, const cv::Mat& src, float scale = 1000.0f) {
    assert(src.type() == CV_32FC1 && "convert_32FC1_to_16UC1: source image of different type from 32FC1");
    const float* sptr = (const float*)src.data;
    int size = src.rows * src.cols;
    const float* send = sptr + size;
    dest.create(src.rows, src.cols, CV_16UC1);
    dest.setTo(cv::Scalar(0));
    unsigned short* dptr = (unsigned short*)dest.data;
    while(sptr < send) {
      if(*sptr >= 1e9f) { *dptr = 0; }
      else { *dptr = scale * (*sptr); }
      ++dptr;
      ++sptr;
    }
  }

};

int main(int argc, char** argv){
  ros::init(argc, argv, "image_dumper");
  ros::NodeHandle nh;
  ImageDumper dumper(nh);

  //ros::spin();

  ros::Rate loop_rate(1);
  while(ros::ok() && keep_going){
    ros::spinOnce();
    loop_rate.sleep();
  }

  ROS_INFO("Terminating image_dumper");

  return 0;
}
