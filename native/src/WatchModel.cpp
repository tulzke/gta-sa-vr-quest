#include "WatchModel.h"
#include "../vendor/cgltf/cgltf.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace savr::watchmodel {
bool Model::Load(const void* bytes,std::size_t size) {
    *this={};
    const auto fail=[&](const char* reason){meshes.clear();materials.clear();images.clear();error=reason;return false;};
    if(!bytes||size<28||size>4*1024*1024)return fail("Invalid watch GLB size");
    cgltf_options options{};cgltf_data* parsed=nullptr;
    if(cgltf_parse(&options,bytes,size,&parsed)!=cgltf_result_success)return fail("Watch GLB parse failed");
    const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(parsed,cgltf_free);
    auto& d=*parsed;
    if(d.file_type!=cgltf_file_type_glb||d.buffers_count!=1||d.buffers[0].uri||
       !d.bin||d.buffers[0].size>d.bin_size||d.extensions_required_count||
       d.animations_count||d.skins_count||d.nodes_count>32||d.meshes_count>16||
       d.materials_count>16||d.images_count>8)return fail("Unsupported watch GLB profile");
    d.buffers[0].data=const_cast<void*>(d.bin);
    for(std::size_t i=0;i<d.buffer_views_count;++i) {
        const auto& v=d.buffer_views[i];
        if(v.buffer!=d.buffers||v.has_meshopt_compression||v.offset>d.buffers[0].size||
           v.size>d.buffers[0].size-v.offset)return fail("Invalid watch buffer view");
    }
    for(std::size_t i=0;i<d.accessors_count;++i) {
        const auto& a=d.accessors[i];const auto width=cgltf_calc_size(a.type,a.component_type);
        if(a.is_sparse||!a.buffer_view||!width||!a.count||a.count>65535||a.stride<width||
           a.offset>a.buffer_view->size||width>a.buffer_view->size-a.offset||
           a.count-1>(a.buffer_view->size-a.offset-width)/a.stride)
            return fail("Invalid watch accessor");
    }
    if(cgltf_validate(&d)!=cgltf_result_success)return fail("Watch GLB validation failed");
    for(std::size_t i=0;i<d.images_count;++i) {
        const auto& image=d.images[i];
        if(image.uri||!image.buffer_view||!image.mime_type||
           (std::strcmp(image.mime_type,"image/png")&&std::strcmp(image.mime_type,"image/jpeg")))
            return fail("Watch textures must be embedded PNG/JPEG");
        const auto* data=static_cast<const unsigned char*>(d.bin)+image.buffer_view->offset;
        images.emplace_back(data,data+image.buffer_view->size);
    }
    const auto imageId=[&](const cgltf_texture_view& view) {
        return view.texture&&view.texture->image ? static_cast<int>(view.texture->image-d.images) : -1;
    };
    for(std::size_t i=0;i<d.materials_count;++i) {
        const auto& p=d.materials[i].pbr_metallic_roughness;Material material;
        std::copy_n(p.base_color_factor,4,material.color.begin());
        material.metallic=p.metallic_factor;material.roughness=p.roughness_factor;
        material.image=imageId(p.base_color_texture);material.metallicImage=imageId(p.metallic_roughness_texture);
        materials.push_back(material);
    }
    int displays=0;
    for(std::size_t n=0;n<d.nodes_count;++n) {
        const auto& node=d.nodes[n];if(!node.mesh)continue;
        if(node.skin||node.weights_count)return fail("Watch must be static");
        float matrix[16];cgltf_node_transform_world(&node,matrix);
        // The approved GLB has identity nodes and a baked hand-relative fit.
        // Reject a changed asset until its transform convention is reviewed.
        for(int k=0;k<16;++k)if(std::abs(matrix[k]-(k%5==0?1.f:0.f))>1e-6f)
            return fail("Watch nodes must use baked hand-relative coordinates");
        const bool display=node.name&&!std::strcmp(node.name,"WatchshellTop_RIS_Shader:PxrDisney2_0");
        for(std::size_t p=0;p<node.mesh->primitives_count;++p) {
            const auto& primitive=node.mesh->primitives[p];
            if(primitive.type!=cgltf_primitive_type_triangles||!primitive.indices||
               primitive.indices->count%3||!primitive.material)return fail("Invalid watch primitive");
            const auto* positions=cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0);
            const auto* normals=cgltf_find_accessor(&primitive,cgltf_attribute_type_normal,0);
            const auto* uv=cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,0);
            if(!positions||!normals||!uv||positions->type!=cgltf_type_vec3||
               normals->type!=cgltf_type_vec3||uv->type!=cgltf_type_vec2||
               positions->count!=normals->count||positions->count!=uv->count)
                return fail("Watch requires positions, normals and UV0");
            Mesh mesh;mesh.material=static_cast<int>(primitive.material-d.materials);mesh.display=display;
            mesh.vertices.resize(positions->count);
            for(std::size_t v=0;v<positions->count;++v) {
                auto& vertex=mesh.vertices[v];
                if(!cgltf_accessor_read_float(positions,v,vertex.position,3)||
                   !cgltf_accessor_read_float(normals,v,vertex.normal,3)||
                   !cgltf_accessor_read_float(uv,v,vertex.uv,2))return fail("Watch vertex read failed");
                for(float x:vertex.position)if(!std::isfinite(x)||std::abs(x)>.5f)return fail("Invalid watch position");
                for(float x:vertex.normal)if(!std::isfinite(x))return fail("Invalid watch normal");
                for(float x:vertex.uv)if(!std::isfinite(x))return fail("Invalid watch UV");
                if(display)for(float x:vertex.uv)if(x<-.0001f||x>1.0001f)return fail("Invalid display UV");
            }
            for(std::size_t i=0;i<primitive.indices->count;++i) {
                const auto index=cgltf_accessor_read_index(primitive.indices,i);
                if(index>=mesh.vertices.size())return fail("Watch index out of bounds");
                mesh.indices.push_back(static_cast<std::uint16_t>(index));
            }
            if(display) {
                ++displays;
                const auto& a=mesh.vertices[mesh.indices[0]];
                const auto& b=mesh.vertices[mesh.indices[1]];
                const auto& c=mesh.vertices[mesh.indices[2]];
                const float u1=b.uv[0]-a.uv[0],v1=b.uv[1]-a.uv[1];
                const float u2=c.uv[0]-a.uv[0],v2=c.uv[1]-a.uv[1];
                const float det=u1*v2-u2*v1;
                if(std::abs(det)<1e-8f)return fail("Degenerate display UV triangle");
                for(int axis=0;axis<3;++axis) {
                    const float e1=b.position[axis]-a.position[axis],e2=c.position[axis]-a.position[axis];
                    right[axis]=(v2*e1-v1*e2)/det;
                    up[axis]=(u2*e1-u1*e2)/det; // glTF V goes down
                    normal[axis]=a.normal[axis];
                }
                for(auto* basis:{&right,&up,&normal}) {
                    float length=0;for(float x:*basis)length+=x*x;
                    if(length<1e-10f||!std::isfinite(length))return fail("Invalid display frame");
                    for(float& x:*basis)x/=std::sqrt(length);
                }
                for(int axis=0;axis<3;++axis) {
                    const auto extremes=std::minmax_element(mesh.vertices.begin(),mesh.vertices.end(),
                        [axis](const Vertex& a,const Vertex& b){return a.position[axis]<b.position[axis];});
                    centre[axis]=(extremes.first->position[axis]+extremes.second->position[axis])*.5f;
                }
            }
            meshes.push_back(std::move(mesh));
        }
    }
    return displays==1&&!meshes.empty() ? true : fail("Watch needs exactly one interface surface");
}
}
