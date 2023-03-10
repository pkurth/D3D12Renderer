#include "pch.h"
#include "nearest_neighbor.h"

#include <nanoflann/nanoflann.hpp>


point_cloud::point_cloud(vec3* positions, uint32 numPositions)
{
    using kd_tree_t = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<float, point_cloud>,
        point_cloud, 3>;

    this->positions = positions;
    this->numPositions = numPositions;

    this->index = new kd_tree_t(3, *this, { 10 });
}

point_cloud::~point_cloud()
{
    using kd_tree_t = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<float, point_cloud>,
        point_cloud, 3>;

    kd_tree_t* index = (kd_tree_t*)this->index;

    delete index;
}

nearest_neighbor_query_result point_cloud::nearestNeighborIndex(vec3 query)
{
    using kd_tree_t = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<float, point_cloud>,
        point_cloud, 3>;

    kd_tree_t* index = (kd_tree_t*)this->index;

    size_t resultIndex;
    float squaredDistance;
    nanoflann::KNNResultSet<float> resultSet(1);
    resultSet.init(&resultIndex, &squaredDistance);

    index->findNeighbors(resultSet, &query.data[0]);

    return { (uint32)resultIndex, squaredDistance };
}
