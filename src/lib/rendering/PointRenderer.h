﻿/**
 * Copyright (c) 2023 Darius Rückert and Linus Franke
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once
#include "saiga/cuda/imageProcessing/image.h"
#include "saiga/cuda/imgui_cuda.h"

#include "NeuralPointCloudCuda.h"
#include "RenderConstants.h"
#include "RenderInfo.h"
#include "config.h"
#include "data/Dataset.h"
#include "data/NeuralScene.h"
#include "data/Settings.h"



class PointRendererCache;

class NeuralRenderInfo : public torch::CustomClassHolder
{
   public:
    NeuralScene* scene;
    std::vector<ReducedImageInfo> images;
    RenderParams params;
    int num_layers;
    bool train;
    int current_epoch;
    CUDA::CudaTimerSystem* timer_system = nullptr;
    PointRendererCache* cache           = nullptr;
};


namespace torch::autograd
{
struct PointRender : public Function<PointRender>
{
    // returns a tensor for every layer
    static variable_list forward(AutogradContext* ctx, Variable texture, Variable background_color, Variable points,
                                 Variable pose_tangents, Variable intrinsics, Variable confidence,
                                 Variable dynamic_refinement, IValue info);

    static variable_list backward(AutogradContext* ctx, variable_list grad_output);
};
}  // namespace torch::autograd


// Render the scene into a batch of images
// Every image is a pyramid of layers in different resolutions
std::vector<torch::Tensor> BlendPointCloud(NeuralRenderInfo* info);


// ==== Internal ====
std::pair<std::vector<torch::Tensor>, std::vector<torch::Tensor>> BlendPointCloudForward(
    torch::autograd::AutogradContext* ctx, NeuralRenderInfo* info);
// derivative towards the texture
torch::autograd::variable_list BlendPointCloudBackward(torch::autograd::AutogradContext* ctx, NeuralRenderInfo* info,
                                                       torch::autograd::variable_list image_gradients);


void ApplyTangentToPose(torch::Tensor tangent, torch::Tensor pose);

struct LayerCuda
{
    ivec2 size  = ivec2(0, 0);
    float scale = 1;


    ImageView<Packtype> BatchView(int batch)
    {
        return ImageView<Packtype>(size(1), size(0),
                                   depth_index_tensor.data_ptr<long>() + batch * depth_index_tensor.stride(0));
    }

    ImageView<float> BatchViewDepth(int batch)
    {
        return ImageView<float>(size(1), size(0), depth.data_ptr<float>() + batch * depth.stride(0));
    }
    ImageView<int> BatchViewCounting(int batch)
    {
        return ImageView<int>(size(1), size(0), counting.data_ptr<int>() + batch * counting.stride(0));
    }
    ImageView<int> BatchViewScannedCounting(int batch)
    {
        return ImageView<int>(size(1), size(0), scanned_counting.data_ptr<int>() + batch * scanned_counting.stride(0));
    }
    ImageView<float> BatchViewWeights(int batch)
    {
        return ImageView<float>(size(1), size(0), weight.data_ptr<float>() + batch * weight.stride(0));
    }


    // for new rendering
    torch::Tensor depth;
    torch::Tensor weight;

    torch::Tensor max_depth;
    torch::Tensor counting;
    // an ascending count structure, using a scan
    torch::Tensor scanned_counting;

    torch::Tensor per_image_atomic_counters;


    // for old rendering
    torch::Tensor depth_index_tensor;
};

class PointRendererCache
{
   public:
    PointRendererCache() {}

    void Build(NeuralRenderInfo* info, bool forward);

    // Allocates cuda memory in tensors. Does not initialize them!!!
    // Call InitializeData() below for that
    void Allocate(NeuralRenderInfo* info, bool forward);
    void InitializeData(bool forward);

    void PushParametersForward();
    void PushParametersBackward();
    DeviceRenderParams PrepareDeviceRenderParams();
    DeviceTexture PrepareDeviceTexture();
    DeviceBackwardParams PrepareDeviceBackwardParams();

    void ProjectPoints(int batch, NeuralPointCloudCuda point_cloud);
    void DepthPrepassMulti(int batch, NeuralPointCloudCuda point_cloud, bool train);
    void RenderForwardMulti(int batch, NeuralPointCloudCuda point_cloud, bool train);

    void CountingPrepassMulti(int batch, NeuralPointCloudCuda point_cloud, bool train);
    void CollectMulti(int batch, NeuralPointCloudCuda point_cloud, std::vector<torch::Tensor> collection_buffers,
                      std::vector<torch::Tensor> data_buffer, bool train);
    void UploadCollectionBuffers(std::vector<torch::Tensor> buffers, std::vector<torch::Tensor> data_buffers,
                                 int batch_num, std::vector<torch::Tensor> atomics = std::vector<torch::Tensor>());
    void SortMulti(int batch, std::vector<torch::Tensor> collection_buffers, bool train);
    void BlendMulti(int batch, DevicePointCloud point_cloud, std::vector<torch::Tensor> collection_buffers,
                    std::vector<torch::Tensor> data_buffer, torch::Tensor background_color, bool train,
                    bool use_environment_map);

    void BlendMultiFuzzy(int batch, DevicePointCloud point_cloud, std::vector<torch::Tensor> collection_buffers,
                         std::vector<torch::Tensor> data_buffer, torch::Tensor background_color, bool train,
                         bool use_environment_map);


    void CountingPrepassMultiBilinear(int batch, NeuralPointCloudCuda point_cloud, bool train);

    void CollectMultiBilinear(int batch, NeuralPointCloudCuda point_cloud,
                              std::vector<torch::Tensor> collection_buffers, std::vector<torch::Tensor> data_buffer,
                              bool train);
    void BlendMultiBilinear(int batch, DevicePointCloud point_cloud, std::vector<torch::Tensor> collection_buffers,
                            std::vector<torch::Tensor> data_buffer, torch::Tensor background_color, bool train,
                            bool use_environment_map);

    void CombineAndFillBlend(int batch, torch::Tensor background_color);

    void BlendBackwards(int batch, NeuralPointCloudCuda point_cloud, std::vector<torch::Tensor> collection_buffers,
                        torch::Tensor background_color, std::vector<torch::Tensor> grad_sum_back_buffers,
                        bool use_environment_map);
    void BlendBackwardsFuzzy(int batch, NeuralPointCloudCuda point_cloud, std::vector<torch::Tensor> collection_buffers,
                             torch::Tensor background_color, std::vector<torch::Tensor> grad_sum_back_buffers,
                             bool use_environment_map);
    void BlendBackwardsBilinear(int batch, NeuralPointCloudCuda point_cloud,
                                std::vector<torch::Tensor> collection_buffers,
                                std::vector<torch::Tensor> per_point_data_buffer, torch::Tensor background_color,
                                std::vector<torch::Tensor> grad_sum_back_buffers, bool use_environment_map);

    void UploadCollectionBuffersBackwards(std::vector<torch::Tensor> buffers, std::vector<torch::Tensor> data_buffer,
                                          std::vector<torch::Tensor> grad_sum_back_buffers, int batch_num);


    void FillSceneGridWithLoss(int batch, NeuralPointCloudCuda point_cloud, std::vector<ReducedImageInfo> images,
                               torch::Tensor loss_img);

    void RenderBackward(int batch, NeuralPointCloudCuda point_cloud);

    void CreateMask(int batch, float background_value);

    void PartialTextureTransferForwardMulti(int batch, NeuralPointCloudCuda point_cloud, bool train);

    void CombineAndFill(int batch, torch::Tensor background_color);
    void CombineAndFillBackward(int batch, torch::Tensor background_color, std::vector<torch::Tensor> gradient);

    enum RenderMode
    {
        FUZZY_DT       = 0,
        FULL_BLEND     = 1,
        FUZZY_BLEND    = 2,
        BILINEAR_BLEND = 3,
        SIZE           = 4
    } render_mode;



    std::vector<LayerCuda> layers_cuda;
    NeuralRenderInfo* info;
    int num_batches;

    torch::Tensor cells_to_cull;
    torch::Tensor random_numbers;

    torch::Tensor gradient_of_forward_pass_x;

    torch::Tensor l1_error_image;



    // [batch, num_points]
    torch::Tensor dropout_points;

    std::vector<torch::Tensor> output_forward;
    std::vector<torch::Tensor> output_forward_depthbuffer;
    std::vector<torch::Tensor> output_forward_blend;

    // [batches, num_points, 2]
    torch::Tensor tmp_point_projections;

    torch::Tensor output_gradient_texture;
    torch::Tensor output_gradient_confidence;
    torch::Tensor output_gradient_background;
    torch::Tensor output_gradient_points;

    torch::Tensor output_gradient_pose_tangent;
    torch::Tensor output_gradient_pose_tangent_count;
    torch::Tensor output_gradient_point_count;

    torch::Tensor output_gradient_intrinsics;
    torch::Tensor output_gradient_intrinsics_count;

    torch::Tensor output_gradient_dynamic_points;
    torch::Tensor output_gradient_dynamic_point_count;


    //std::vector<torch::Tensor> point_dynamic_gradients;

    std::vector<torch::Tensor> image_gradients;



    // Variables used to check if we have to reallocate
    // [num_points, layers, batch_size, h, w]
    std::vector<int> cache_size = {0, 0, 0, 0};
    bool cache_has_forward      = false;
    bool cache_has_backward     = false;
};