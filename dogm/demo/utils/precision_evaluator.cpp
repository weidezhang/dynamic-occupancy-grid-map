// Copyright (c) 2020 Michael Koesel and respective contributors
// SPDX-License-Identifier: MIT
// See accompanying LICENSE file for detailed information

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "dbscan.h"
#include "precision_evaluator.h"

const float kMaximumAssignmentDistance = 5.0f;
const float kMaximumDbscanNeighborDistance = 3.0f;
const int kMinimumNumberOfNeighbors = 5;

static PointWithVelocity computeError(const PointWithVelocity& cluster_mean, const Vehicle& vehicle)
{
    PointWithVelocity error{};
    error.x += cluster_mean.x - vehicle.pos[0];
    error.y += cluster_mean.y - vehicle.pos[1];
    error.v_x += cluster_mean.v_x - vehicle.vel[0];
    error.v_y += cluster_mean.v_y - vehicle.vel[1];
    return error;
}

static Clusters<dogm::GridCell> computeDbscanClusters(const std::vector<Point<dogm::GridCell>>& cells_with_velocity)
{
    const DBSCAN<dogm::GridCell> dbscan(kMaximumDbscanNeighborDistance, kMinimumNumberOfNeighbors);
    return dbscan.cluster(cells_with_velocity);
}

PrecisionEvaluator::PrecisionEvaluator(const SimulationData _sim_data, const float _resolution, const float _grid_size)
    : sim_data{_sim_data}, resolution{_resolution}, grid_size{_grid_size}
{
    number_of_detections = 0;
    number_of_unassigned_detections = 0;
}

void PrecisionEvaluator::evaluateAndStoreStep(int simulation_step_index,
                                              const std::vector<Point<dogm::GridCell>>& cells_with_velocity,
                                              bool print_current_precision)
{
    const auto groundtruth_vehicles = sim_data[simulation_step_index].vehicles;
    if (cells_with_velocity.size() > 0 && groundtruth_vehicles.size() > 0)
    {
        const auto clusters = computeDbscanClusters(cells_with_velocity);
        int cluster_id = 0;
        for (const auto& cluster : clusters)
        {
            PointWithVelocity cluster_mean = computeClusterMean(cluster);

            std::vector<Vehicle> matching_groundtruth_vehicles{};
            for (const auto& vehicle : groundtruth_vehicles)
            {
                const float distance =
                    sqrtf(powf(cluster_mean.x - vehicle.pos[0], 2.0f) + powf(cluster_mean.y - vehicle.pos[1], 2.0f));
                if (distance < kMaximumAssignmentDistance)
                {
                    matching_groundtruth_vehicles.push_back(vehicle);
                }
            }

            if (matching_groundtruth_vehicles.size() == 0)
            {
                ++number_of_unassigned_detections;
                continue;
            }

            std::sort(matching_groundtruth_vehicles.begin(), matching_groundtruth_vehicles.end(),
                      [&cluster_mean](const Vehicle& a, const Vehicle& b) {
                          const float distance_a =
                              sqrtf(powf(cluster_mean.x - a.pos[0], 2.0f) + powf(cluster_mean.y - a.pos[1], 2.0f));
                          const float distance_b =
                              sqrtf(powf(cluster_mean.x - b.pos[0], 2.0f) + powf(cluster_mean.y - b.pos[1], 2.0f));
                          return distance_a < distance_b;
                      });

            const auto closest_vehicle = matching_groundtruth_vehicles[0];

            const auto error = computeError(cluster_mean, closest_vehicle);

            accumulateErrors(error);

            if (print_current_precision)
            {
                std::cout << std::setprecision(2);
                std::cout << "\nCluster ID=" << cluster_id << "\n";
                std::cout << "Vel. Err.: " << error.v_x << " " << error.v_y << ", Pos. Err.: " << error.x << " "
                          << error.y << "\n";
            }
            cluster_id++;
        }
    }
}

PointWithVelocity PrecisionEvaluator::computeClusterMean(const Cluster<dogm::GridCell>& cluster)
{
    // TODO check if median is more precise than mean
    PointWithVelocity cluster_mean;
    for (auto& point : cluster)
    {
        cluster_mean.x += point.x;
        cluster_mean.y += point.y;
        cluster_mean.v_x += point.data.mean_x_vel;
        cluster_mean.v_y += point.data.mean_y_vel;
    }

    cluster_mean.x = (cluster_mean.x / cluster.size()) * resolution;
    cluster_mean.y = (cluster_mean.y / cluster.size()) * resolution;
    cluster_mean.v_x = (cluster_mean.v_x / cluster.size()) * resolution;
    cluster_mean.v_y = (cluster_mean.v_y / cluster.size()) * resolution;

    // y as grid index is pointing downwards from top left corner.
    // y in world coordinates is pointing upwards from bottom left corner.
    // Therefore, vectors (velocity) just needs to be inverted. Positions (mean) must be inverted and translated.
    cluster_mean.v_y = -cluster_mean.v_y;
    cluster_mean.y = grid_size - cluster_mean.y;

    return cluster_mean;
}

void PrecisionEvaluator::accumulateErrors(const PointWithVelocity& error)
{
    cumulative_error.x += std::abs(error.x);
    cumulative_error.y += std::abs(error.y);
    cumulative_error.v_x += std::abs(error.v_x);
    cumulative_error.v_y += std::abs(error.v_y);
    ++number_of_detections;
}

void PrecisionEvaluator::printSummary()
{
    std::cout << "\nMean absolute errors (x,y): \n";
    std::cout << "Position: " << cumulative_error.x / number_of_detections << " "
              << cumulative_error.y / number_of_detections << "\n";
    std::cout << "Velocity: " << cumulative_error.v_x / number_of_detections << " "
              << cumulative_error.v_y / number_of_detections << "\n\n";
    std::cout << "Detections unassigned by evaluator: " << number_of_unassigned_detections << "\n";
    std::cout << "Maximum possible detections: " << sim_data[0].vehicles.size() * sim_data.size() << "\n";
}
