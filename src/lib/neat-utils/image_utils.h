/**
 * Copyright (c) 2023 Darius Rückert and Linus Franke
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */
#pragma once
#include "saiga/core/Core.h"
#include "saiga/core/camera/HDR.h"
#include "saiga/core/imgui/imgui.h"
#include "saiga/core/math/CoordinateSystems.h"
#include "saiga/core/sophus/Sophus.h"
#include "saiga/core/util/FileSystem.h"
#include "saiga/core/util/directory.h"
#include "saiga/core/util/file.h"
#include "saiga/core/util/tostring.h"
#include "saiga/vision/VisionIncludes.h"
#include "saiga/vision/cameraModel/CameraModel.h"
#include "saiga/vision/cameraModel/Distortion.h"
#include "saiga/vision/cameraModel/OCam.h"
#include "saiga/vision/torch/TorchHelper.h"

using namespace Saiga;

TemplatedImage<float> UndistortImage(ImageView<float> img, IntrinsicsPinholef K, Distortionf D,
                                     IntrinsicsPinholef target_K, ivec2 sizes_image, float scene_scale = 1.f,
                                     unsigned int border_pixels = 16);

TemplatedImage<float> UndistortOCAMImage(ImageView<float> img, IntrinsicsPinholef K, IntrinsicsPinholef targetK,
                                         OCam<double> ocam, vec2 size_of_ocam_target_image, float scene_scale = 0.5f,
                                         unsigned int border_pixels = 16);

torch::Tensor process_l1_image(torch::Tensor l1_img);
torch::Tensor process_ssim_image(torch::Tensor ssim_map);
torch::Tensor process_l2_image(torch::Tensor l2_map);
void write16bitImg(TemplatedImage<float> img, std::string path);
