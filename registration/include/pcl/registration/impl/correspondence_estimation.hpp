/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */
#ifndef PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_H_
#define PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_H_

#include <pcl/common/concatenate.h>
#include <pcl/registration/correspondence_estimation.h>
#include <pcl/common/io.h>

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointSource, typename PointTarget> inline void
pcl::registration::CorrespondenceEstimation<PointSource, PointTarget>::setInputTarget (
    const PointCloudTargetConstPtr &cloud)
{
  if (cloud->points.empty ())
  {
    PCL_ERROR ("[pcl::%s::setInputTarget] Invalid or empty point cloud dataset given!\n", getClassName ().c_str ());
    return;
  }
  target_ = cloud;
  tree_->setInputCloud (target_);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointSource, typename PointTarget> void
pcl::registration::CorrespondenceEstimation<PointSource, PointTarget>::determineCorrespondences (
    pcl::Correspondences &correspondences, double max_distance)
{
  typedef typename pcl::traits::fieldList<PointTarget>::type FieldListTarget;

  if (!initCompute ())
    return;

  if (!target_)
  {
    PCL_WARN ("[pcl::%s::compute] No input target dataset was given!\n", getClassName ().c_str ());
    return;
  }

  double max_dist_sqr = max_distance * max_distance;

  correspondences.resize (indices_->size ());
  std::vector<int> index (1);
  std::vector<float> distance (1);
  pcl::Correspondence corr;
  unsigned int nr_valid_correspondences = 0;
  
  // Check if the template types are the same. If true, avoid a copy.
  // Both point types MUST be registered using the POINT_CLOUD_REGISTER_POINT_STRUCT macro!
  if (!isSamePointType<PointSource, PointTarget> ())
  {
    for (size_t i = 0; i < indices_->size (); ++i)
    {
      if (!tree_->nearestKSearch (input_->points[(*indices_)[i]], 1, index, distance))
        continue;
      if (distance[0] > max_dist_sqr)
        continue;

      corr.index_query = static_cast<int> (i);
      corr.index_match = index[0];
      corr.distance = distance[0];
      correspondences[nr_valid_correspondences++] = corr;
    }
  }
  else
  {
    for (size_t i = 0; i < indices_->size (); ++i)
    {
      PointTarget pt;
      // Copy the source data to a target PointTarget format so we can search in the tree
      pcl::for_each_type <FieldListTarget> (pcl::NdConcatenateFunctor <PointSource, PointTarget> (
            input_->points[(*indices_)[i]], 
            pt));

      if (!tree_->nearestKSearch (pt, 1, index, distance))
        continue;
      
      if (distance[0] > max_dist_sqr)
        continue;

      corr.index_query = static_cast<int> (i);
      corr.index_match = index[0];
      corr.distance = distance[0];
      correspondences[nr_valid_correspondences++] = corr;
    }
  }
  correspondences.resize (nr_valid_correspondences);
  deinitCompute ();
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointSource, typename PointTarget> void
pcl::registration::CorrespondenceEstimation<PointSource, PointTarget>::determineReciprocalCorrespondences (
    pcl::Correspondences &correspondences, double max_distance)
{
  typedef typename pcl::traits::fieldList<PointSource>::type FieldListSource;
  typedef typename pcl::traits::fieldList<PointTarget>::type FieldListTarget;
  typedef typename pcl::intersect<FieldListSource, FieldListTarget>::type FieldList;
  
  if (!initCompute ())
    return;

  if (!target_)
  {
    PCL_WARN ("[pcl::%s::compute] No input target dataset was given!\n", getClassName ().c_str ());
    return;
  }

  // setup tree for reciprocal search
  pcl::KdTreeFLANN<PointSource> tree_reciprocal;
  tree_reciprocal.setInputCloud (input_, indices_);

  double max_dist_sqr = max_distance * max_distance;

  correspondences.resize (indices_->size());
  std::vector<int> index (1);
  std::vector<float> distance (1);
  std::vector<int> index_reciprocal (1);
  std::vector<float> distance_reciprocal (1);
  pcl::Correspondence corr;
  unsigned int nr_valid_correspondences = 0;

  // Check if the template types are the same. If true, avoid a copy.
  // Both point types MUST be registered using the POINT_CLOUD_REGISTER_POINT_STRUCT macro!
  if (!isSamePointType<PointSource, PointTarget> ())
  {
    for (size_t i = 0; i < indices_->size (); ++i)
    {
      tree_->nearestKSearch (input_->points[(*indices_)[i]], 1, index, distance);
      if (distance[0] > max_dist_sqr)
        continue;

      tree_reciprocal.nearestKSearch (target_->points[index[0]], 1, index_reciprocal, distance_reciprocal);
      if (distance_reciprocal[0] > max_dist_sqr)
        continue;

      if ((*indices_)[i] != index_reciprocal[0])
        continue;

      corr.index_query = (*indices_)[i];
      corr.index_match = index[0];
      corr.distance = distance[0];
      correspondences[nr_valid_correspondences++] = corr;
    }
  }
  else
  {
    for (size_t i = 0; i < indices_->size (); ++i)
    {
      // Copy the source data to a target PointTarget format so we can search in the tree
      PointTarget pt_src;
      pcl::for_each_type <FieldList> (pcl::NdConcatenateFunctor <PointSource, PointTarget> (
            input_->points[(*indices_)[i]], 
            pt_src));

      tree_->nearestKSearch (pt_src, 1, index, distance);
      if (distance[0] > max_dist_sqr)
        continue;

      // Copy the target data to a target PointSource format so we can search in the tree_reciprocal
      PointSource pt_tgt;
      pcl::for_each_type <FieldList> (pcl::NdConcatenateFunctor <PointTarget, PointSource> (
            target_->points[index[0]],
            pt_tgt));

      tree_reciprocal.nearestKSearch (pt_tgt, 1, index_reciprocal, distance_reciprocal);
      if (distance_reciprocal[0] > max_dist_sqr)
        continue;

      if ((*indices_)[i] != index_reciprocal[0])
        continue;

      corr.index_query = (*indices_)[i];
      corr.index_match = index[0];
      corr.distance = distance[0];
      correspondences[nr_valid_correspondences++] = corr;
    }
  }
  correspondences.resize (nr_valid_correspondences);
  deinitCompute ();
}

//#define PCL_INSTANTIATE_CorrespondenceEstimation(T,U) template class PCL_EXPORTS pcl::registration::CorrespondenceEstimation<T,U>;

#endif /* PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_H_ */
