OBJNB=a008052
SCENE_BASE=scenes/a008052


build/bin/colmap2adop --sparse_dir $SCENE_BASE/sparse/0/ \
    --image_dir $SCENE_BASE/multi_views/ \
    --point_cloud_file $SCENE_BASE/dense_point_cloud.ply \
    --output_path $SCENE_BASE/test \
    --scale_intrinsics 1 --render_scale 1 
